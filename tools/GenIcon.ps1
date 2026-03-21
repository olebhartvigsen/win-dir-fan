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

        float pad = size * 0.06f;
        float w   = size - pad * 2;
        float h   = size - pad * 2;

        // Folder (behind documents)
        {
            float fW   = w * 0.88f;
            float fH   = h * 0.48f;
            float fx   = pad + (w - fW) / 2;
            float fy   = pad + h - fH - h * 0.04f;
            float r    = size * 0.04f;
            float tabW = fW * 0.35f;
            float tabH = fH * 0.16f;

            GraphicsPath fp = new GraphicsPath();
            fp.AddArc(fx, fy - tabH, r, r, 180, 90);
            fp.AddLine(fx + r, fy - tabH, fx + tabW - r, fy - tabH);
            fp.AddArc(fx + tabW - r, fy - tabH, r, r, 270, 90);
            fp.AddLine(fx + tabW, fy - tabH + r, fx + tabW + tabH * 0.6f, fy);
            fp.AddLine(fx + tabW + tabH * 0.6f, fy, fx + fW - r, fy);
            fp.AddArc(fx + fW - r, fy, r, r, 270, 90);
            fp.AddLine(fx + fW, fy + r, fx + fW, fy + fH - r);
            fp.AddArc(fx + fW - r, fy + fH - r, r, r, 0, 90);
            fp.AddLine(fx + fW - r, fy + fH, fx + r, fy + fH);
            fp.AddArc(fx, fy + fH - r, r, r, 90, 90);
            fp.CloseFigure();

            using (SolidBrush s = new SolidBrush(Color.FromArgb(90, 0, 0, 0)))
            { g.TranslateTransform(3, 4); g.FillPath(s, fp); g.TranslateTransform(-3, -4); }

            using (LinearGradientBrush grad = new LinearGradientBrush(
                new PointF(fx, fy - tabH), new PointF(fx, fy + fH),
                Color.FromArgb(255, 210, 60), Color.FromArgb(225, 148, 10)))
            { g.FillPath(grad, fp); }

            using (Pen pen = new Pen(Color.FromArgb(175, 100, 5), size * 0.022f))
            { g.DrawPath(pen, fp); }

            fp.Dispose();
        }

        // Back document (tilted right)
        DrawDocument(g, w, h, pad, size,
            pad + w * 0.55f, pad + h * 0.30f, 12f,
            Color.FromArgb(232, 236, 245),
            Color.FromArgb(80, 85, 100),
            Color.FromArgb(155, 160, 178));

        // Front document (tilted left)
        DrawDocument(g, w, h, pad, size,
            pad + w * 0.42f, pad + h * 0.28f, -8f,
            Color.White,
            Color.FromArgb(70, 75, 90),
            Color.FromArgb(148, 153, 170));

        MemoryStream ms = new MemoryStream();
        bmp.Save(ms, ImageFormat.Png);
        byte[] result = ms.ToArray();
        ms.Dispose();
        g.Dispose();
        bmp.Dispose();
        return result;
    }

    private static void DrawDocument(Graphics g, float w, float h, float pad, int size,
        float cx, float cy, float angle, Color fillColor, Color penColor, Color lineColor)
    {
        float docW   = w * 0.52f;
        float docH   = h * 0.62f;
        float corner = size * 0.04f;
        float fold   = size * 0.09f;

        Matrix mx = new Matrix();
        mx.RotateAt(angle, new PointF(cx, cy));
        g.Transform = mx;

        RectangleF rect = new RectangleF(cx - docW / 2, cy - docH / 2 + h * 0.02f, docW, docH);
        GraphicsPath path = RoundedDocPath(rect, corner, fold);

        using (SolidBrush s = new SolidBrush(Color.FromArgb(90, 0, 0, 0)))
        { g.TranslateTransform(3, 4); g.FillPath(s, path); g.TranslateTransform(-3, -4); }

        using (SolidBrush fill = new SolidBrush(fillColor))
        { g.FillPath(fill, path); }

        using (Pen pen = new Pen(penColor, size * 0.022f))
        { g.DrawPath(pen, path); }

        using (Pen linePen = new Pen(lineColor, size * 0.022f))
        {
            float lx = rect.X + rect.Width * 0.15f;
            float lw = rect.Width * 0.7f;
            for (int i = 0; i < 3; i++)
            {
                float ly = rect.Y + rect.Height * (0.40f + i * 0.14f);
                g.DrawLine(linePen, lx, ly, lx + lw, ly);
            }
        }

        path.Dispose();
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
