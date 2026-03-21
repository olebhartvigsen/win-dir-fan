<#
.SYNOPSIS
    Generates app.ico for FanFolderApp using the same visual style as the runtime icon.
.DESCRIPTION
    Renders the "stack" icon (folder with two documents) at 16, 32, 48, and 256 px,
    then writes a multi-size .ico file to FanFolderApp\app.ico.
    Run this script whenever the icon design changes.
#>
param(
    [string]$OutputPath = (Join-Path $PSScriptRoot "..\FanFolderApp\app.ico")
)

Add-Type -AssemblyName System.Drawing

$OutputPath = [System.IO.Path]::GetFullPath($OutputPath)
Write-Host "Generating icon -> $OutputPath" -ForegroundColor Cyan

Add-Type -Language CSharp -ReferencedAssemblies @(
    "System.Drawing",
    "System.Drawing.Primitives"
) -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.IO;
using System.Collections.Generic;

public static class IconGen
{
    public static byte[] CreateStackIconPng(int size)
    {
        Bitmap bmp = new Bitmap(size, size, PixelFormat.Format32bppArgb);
        Graphics g = Graphics.FromImage(bmp);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.Clear(Color.Transparent);

        float cx     = size * 0.50f;
        float pivotY = size * 0.86f;
        float dW     = size * 0.46f;
        float dH     = size * 0.75f;
        float cr     = size * 0.04f;
        float fold   = size * 0.11f;

        // Left (sky blue) — behind centre
        DrawFanDoc(g, cx, pivotY, dW, dH * 0.96f, cr, fold, size,
            -22f,
            Color.FromArgb(60, 165, 252),
            Color.FromArgb(15, 75, 170),
            Color.FromArgb(120, 175, 235));

        // Right (amber) — behind centre
        DrawFanDoc(g, cx, pivotY, dW, dH * 0.96f, cr, fold, size,
            22f,
            Color.FromArgb(255, 198, 40),
            Color.FromArgb(160, 96, 5),
            Color.FromArgb(205, 162, 70));

        // Centre (white) — front
        DrawFanDoc(g, cx, pivotY, dW * 1.06f, dH, cr, fold, size,
            0f,
            Color.White,
            Color.FromArgb(35, 40, 58),
            Color.FromArgb(162, 165, 180));

        MemoryStream ms = new MemoryStream();
        bmp.Save(ms, ImageFormat.Png);
        byte[] result = ms.ToArray();
        ms.Dispose();
        g.Dispose();
        bmp.Dispose();
        return result;
    }

    private static void DrawFanDoc(Graphics g,
        float pivotX, float pivotY, float docW, float docH,
        float cr, float fold, int size,
        float angle, Color fillColor, Color borderColor, Color lineColor)
    {
        Matrix mx = new Matrix();
        mx.RotateAt(angle, new PointF(pivotX, pivotY));
        g.Transform = mx;

        float rx = pivotX - docW / 2f;
        float ry = pivotY - docH;
        RectangleF rect = new RectangleF(rx, ry, docW, docH);

        GraphicsPath sp = RoundedDocPath(rect, cr, fold);
        using (SolidBrush sh = new SolidBrush(Color.FromArgb(90, 0, 0, 0)))
        { g.TranslateTransform(3, 4); g.FillPath(sh, sp); g.TranslateTransform(-3, -4); }
        sp.Dispose();

        GraphicsPath dp = RoundedDocPath(rect, cr, fold);
        using (SolidBrush fb = new SolidBrush(fillColor))
        { g.FillPath(fb, dp); }
        using (Pen bp = new Pen(borderColor, size * 0.030f))
        { g.DrawPath(bp, dp); }
        dp.Dispose();

        using (Pen lp = new Pen(lineColor, size * 0.026f))
        {
            float lx = rect.X + rect.Width * 0.14f;
            float lw = rect.Width * 0.58f;
            for (int i = 0; i < 3; i++)
            {
                float ly = rect.Y + rect.Height * (0.36f + i * 0.14f);
                g.DrawLine(lp, lx, ly, lx + lw, ly);
            }
        }

        mx.Dispose();
        g.ResetTransform();
    }

    private static GraphicsPath RoundedDocPath(RectangleF rect, float cr, float fold)
    {
        GraphicsPath p = new GraphicsPath();
        float x = rect.X, y = rect.Y, pw = rect.Width, ph = rect.Height;
        p.AddArc(x, y, cr, cr, 180, 90);
        p.AddLine(x + cr, y, x + pw - fold, y);
        p.AddLine(x + pw - fold, y, x + pw, y + fold);
        p.AddLine(x + pw, y + fold, x + pw, y + ph - cr);
        p.AddArc(x + pw - cr, y + ph - cr, cr, cr, 0, 90);
        p.AddLine(x + pw - cr, y + ph, x + cr, y + ph);
        p.AddArc(x, y + ph - cr, cr, cr, 90, 90);
        p.CloseFigure();
        return p;
    }

    public static void SaveIco(string outputPath, int[] sizes)
    {
        List<byte[]> pngs = new List<byte[]>();
        foreach (int sz in sizes)
            pngs.Add(CreateStackIconPng(sz));

        int dataOffset = 6 + sizes.Length * 16;
        FileStream fs = File.Create(outputPath);
        BinaryWriter bw = new BinaryWriter(fs);

        // ICONDIR header
        bw.Write((ushort)0);             // reserved
        bw.Write((ushort)1);             // type: icon
        bw.Write((ushort)sizes.Length);  // image count

        // ICONDIRENTRY per size
        int offset = dataOffset;
        for (int i = 0; i < sizes.Length; i++)
        {
            int sz = sizes[i];
            bw.Write((byte)(sz == 256 ? 0 : sz)); // width  (0 means 256)
            bw.Write((byte)(sz == 256 ? 0 : sz)); // height (0 means 256)
            bw.Write((byte)0);    // color count
            bw.Write((byte)0);    // reserved
            bw.Write((ushort)1);  // planes
            bw.Write((ushort)32); // bit depth
            bw.Write((uint)pngs[i].Length);
            bw.Write((uint)offset);
            offset += pngs[i].Length;
        }

        foreach (byte[] png in pngs)
            bw.Write(png);

        bw.Dispose();
        fs.Dispose();
    }
}
'@

[IconGen]::SaveIco($OutputPath, @(16, 32, 48, 256))
Write-Host "Done: $OutputPath" -ForegroundColor Green
