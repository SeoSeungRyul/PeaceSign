param([string]$SourcePath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'SourceArt/Gathering/TreeSprites/TreeSet-long-trunks-20261103.png'))
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$refs = if ([AppContext]::GetData('TRUSTED_PLATFORM_ASSEMBLIES')) {
    @(([AppContext]::GetData('TRUSTED_PLATFORM_ASSEMBLIES') -split [IO.Path]::PathSeparator) + [Drawing.Bitmap].Assembly.Location | Select-Object -Unique)
} else { @('System.Drawing') }
Add-Type -ReferencedAssemblies $refs -TypeDefinition @'
using System;
using System.IO;
using System.Drawing;
using System.Drawing.Imaging;
using System.Drawing.Drawing2D;
public static class PSTreeSpriteExport {
 public static void Run(string source, string output) {
  string[] names={"Oak","Maple","Cherry","Palm","Cactus","Birch"};
  int[][] edges={new[]{0,433,835,1254},new[]{0,450,815,1254}};
  Directory.CreateDirectory(output);
  using(Bitmap src=new Bitmap(source)) {
   if(src.Width!=1254 || src.Height!=1254) throw new Exception("Unexpected source size; review crop regions before exporting.");
   for(int i=0;i<6;i++){
    int row=i/3,col=i%3,x0=edges[row][col],x1=edges[row][col+1],y0=row==0?0:575,y1=row==0?575:1254;
    int l=x1,t=y1,r=x0,b=y0;
    for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++)if(src.GetPixel(x,y).A>128){l=Math.Min(l,x);t=Math.Min(t,y);r=Math.Max(r,x);b=Math.Max(b,y);}
    int footLeft=r,footRight=l;
    for(int y=Math.Max(t,b-(b-t)/12);y<=b;y++)for(int x=l;x<=r;x++)if(src.GetPixel(x,y).A>128){footLeft=Math.Min(footLeft,x);footRight=Math.Max(footRight,x);}
    double rootX=(footLeft+footRight)/2.0;
    l=Math.Max(x0,l-2);t=Math.Max(y0,t-2);r=Math.Min(x1-1,r+2);b=Math.Min(y1-1,b+2);
    double scale=Math.Min(224.0/(b-t+1),112.0/Math.Max(rootX-l,r-rootX+1));
    int w=(int)Math.Round((r-l+1)*scale),h=(int)Math.Round((b-t+1)*scale);
    int dx=(int)Math.Round(128-(rootX-l)*scale),dy=240-h;
    using(Bitmap cut=src.Clone(new Rectangle(l,t,r-l+1,b-t+1),PixelFormat.Format32bppArgb))
    using(Bitmap icon=new Bitmap(256,256,PixelFormat.Format32bppArgb))
    using(Graphics g=Graphics.FromImage(icon))
    using(ImageAttributes a=new ImageAttributes()){
     g.Clear(Color.Transparent);g.CompositingMode=CompositingMode.SourceCopy;g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.PixelOffsetMode=PixelOffsetMode.HighQuality;
     a.SetWrapMode(WrapMode.TileFlipXY);g.DrawImage(cut,new Rectangle(dx,dy,w,h),0,0,cut.Width,cut.Height,GraphicsUnit.Pixel,a);
     icon.Save(Path.Combine(output,"T_Tree_"+names[i]+".png"),ImageFormat.Png);
    }
    Console.WriteLine(names[i]+" -> 256x256, root pivot (128,240)");
   }
  }
 }
}
'@
$outputDir = Join-Path (Split-Path -Parent $SourcePath) '256'
if (Test-Path -LiteralPath $outputDir) {throw 'Output directory exists; preserve existing exports.'}
[PSTreeSpriteExport]::Run($SourcePath,$outputDir)
Compress-Archive -LiteralPath (Get-ChildItem -LiteralPath $outputDir -Filter '*.png').FullName -DestinationPath (Join-Path (Split-Path -Parent $SourcePath) 'TreeSprites-256.zip')
