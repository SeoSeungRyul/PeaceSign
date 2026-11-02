"""Extract Item / TreeCutDrop / TreeShakeDrop to UE CSV without modifying the workbook.

Run with a Python environment containing openpyxl. Arguments are documented in --help.
Only cached formula values are read; this script never executes workbook formulas.
"""
import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path

import openpyxl


def identifier(value, optional=False):
    if value is None or str(value).strip() in ('', 'None'):
        if optional:
            return 'None'
        raise ValueError('Missing ID')
    if isinstance(value, (int, float)):
        if not math.isfinite(float(value)) or int(value) != value:
            raise ValueError(f'Invalid numeric ID: {value}')
        return '0' if value == 0 else str(int(value)).zfill(6)
    text = str(value).strip()
    if text == '0':
        return text
    if not text.isdecimal() or len(text) != 6:
        raise ValueError(f'ID must contain exactly six digits: {text!r}')
    return text


def integer(value, default=None):
    if value is None or value == '':
        if default is not None:
            return default
        raise ValueError('Missing integer')
    number = float(value)
    if not math.isfinite(number) or int(number) != number:
        raise ValueError(f'Expected an integer, got {value!r}')
    return int(number)


def export(source, output):
    book = openpyxl.load_workbook(source, read_only=True, data_only=True)
    formula_book = openpyxl.load_workbook(source, read_only=True, data_only=False)
    result = {}
    item_rows = []
    ids = set()
    tree_icon_ids = {f'030{kind}{species:02d}' for kind in (1, 2) for species in range(1, 7)}
    try:
        for row_number, values in enumerate(book['Item'].iter_rows(min_row=4, max_col=8, values_only=True), 4):
            if all(value is None for value in values):
                continue
            item_id = identifier(values[0])
            if item_id in ids:
                raise ValueError(f'Item row {row_number}: duplicate ID {item_id}')
            ids.add(item_id)
            discard = values[7]
            if discard not in (True, False, 0, 1):
                raise ValueError(f'Item H{row_number}: Isdiscard must be True or False')
            name = values[1]
            if not isinstance(name, str) or not name.strip():
                raise ValueError(f'Item B{row_number}: missing item name')
            # Local inventory artwork uses one Icon row per log/seed. The workbook
            # still shares a placeholder IconID, so retain this mapping on export.
            icon_id = item_id if item_id in tree_icon_ids else identifier(values[3], True)
            item_rows.append([item_id, name, values[2] or '', icon_id,
                              integer(values[4], -1), integer(values[5], -1), identifier(values[6], True),
                              'True' if discard else 'False'])
        result['Item'] = [['---', 'Name', 'Description', 'IconID', 'Sellp', 'BuyP', 'Store', 'Isdiscard']] + item_rows

        for sheet_name in ('TreeCutDrop', 'TreeShakeDrop'):
            rows, row_names, totals = [], set(), defaultdict(float)
            for row_number, values in enumerate(book[sheet_name].iter_rows(min_row=4, max_col=4, values_only=True), 4):
                if all(value is None for value in values):
                    continue
                drop_id, item_id = identifier(values[0]), identifier(values[1])
                rate = values[2]
                if rate is None:
                    formula = formula_book[sheet_name][f'C{row_number}'].value
                    raise ValueError(f'{sheet_name}!C{row_number}: no calculated value for {formula!r}; recalculate and save in Excel')
                rate = float(rate)
                count = integer(values[3])
                if not math.isfinite(rate) or not 0 <= rate <= 1 or count < 0:
                    raise ValueError(f'{sheet_name} row {row_number}: invalid DropRate or Count')
                if item_id == '0':
                    if sheet_name != 'TreeShakeDrop' or count != 0:
                        raise ValueError(f'{sheet_name} row {row_number}: invalid blank reward')
                elif item_id not in ids:
                    raise ValueError(f'{sheet_name} row {row_number}: unknown ItemID {item_id}')
                row_name = f'{drop_id}_{item_id}_{count}'
                if row_name in row_names:
                    raise ValueError(f'{sheet_name}: duplicate candidate {row_name}')
                row_names.add(row_name)
                totals[(drop_id, item_id if sheet_name == 'TreeCutDrop' else '')] += rate
                rows.append([row_name, drop_id, item_id, format(rate, '.12g'), count])
            for key, total in totals.items():
                if not math.isclose(total, 1.0, abs_tol=1e-5):
                    raise ValueError(f'{sheet_name} {key}: probability sum is {total}, expected 1')
            result[sheet_name] = [['---', 'ID', 'ItemID', 'DropRate', 'Count']] + rows
    finally:
        book.close()
        formula_book.close()
    # Validate everything before writing any output.
    output.mkdir(parents=True, exist_ok=True)
    for name, rows in result.items():
        with (output / f'{name}.csv').open('w', encoding='utf-8-sig', newline='') as stream:
            csv.writer(stream).writerows(rows)
        print(f'{name}.csv: {len(rows) - 1} rows')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('workbook', type=Path)
    parser.add_argument('--output', type=Path, default=Path(__file__).resolve().parents[1] / 'SourceData' / 'TreeTables')
    args = parser.parse_args()
    export(args.workbook, args.output)
