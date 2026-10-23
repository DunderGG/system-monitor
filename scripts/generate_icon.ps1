<#
.SYNOPSIS
Draws the application icon and writes it as verdandi.ico, verdandi.svg and
verdandi_256.png, with two social preview images for GitHub.

.DESCRIPTION
The icon is a chart line that runs flat, dips into a V and runs flat again,
amber on a dark indigo tile: the V of Verdandi, drawn as the kind of trace the
monitor shows (DC-130). The script holds the icon's geometry and draws it with
GDI+ (System.Drawing), so it needs nothing beyond Windows PowerShell.

verdandi.ico holds the sizes that Windows picks from, from 16 px (a title bar
at 100% scale) to 256 px (Explorer's largest icons). The 16, 20 and 24 px images
have geometry of their own, placed on whole and half pixels so that their edges
stay sharp; the other sizes scale the 256 px design. Each image below 256 px is
an uncompressed 32-bit bitmap with an alpha channel, and the 256 px image is a
PNG.

verdandi.svg is the 256 px design, for pages and documents, and
verdandi_256.png the 256 px image, for places that take no SVG.

social_preview.png is a 1280 x 640 px banner of the icon, the name and a line
that says what the monitor is, on a darker indigo. GitHub shows it when a link
to the repository is shared, once it is uploaded in the repository's settings
(Settings, General, Social preview), which take PNG, JPG or GIF but not SVG.
social_preview_wordmark.png is an alternative to it: the name, in bold, with
the icon's line in place of its V, and the same line below it, with no tile.

.PARAMETER OutputDirectory
Where the files go. By default it is the repository's resources directory.

.PARAMETER PngDirectory
If given, each size is also saved there as verdandi_<size>.png, to look at.

.EXAMPLE
.\scripts\generate_icon.ps1

.EXAMPLE
.\scripts\generate_icon.ps1 -PngDirectory build\icon
#>
[CmdletBinding()]
param(
    [string]$OutputDirectory = "",
    [string]$PngDirectory = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$projectRoot = Split-Path -Parent $PSScriptRoot
if ($OutputDirectory -eq "")
{
    $OutputDirectory = Join-Path $projectRoot "resources"
}
$OutputDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)

$tileColor = "#1B2340"
$traceColor = "#F4B740"
# The flat parts of the line are white at this opacity, over the tile.
$flatOpacity = 0.55
$sizes = @(16, 20, 24, 32, 40, 48, 64, 96, 256)

# The social preview: the icon on a darker indigo than its tile, so that the
# tile's shape still shows, and the text in the README's words.
$previewBackground = "#0F1426"
$previewTitleColor = "#F2F4FA"
$previewTaglineColor = "#A9B3D1"
$previewTitle = "Verdandi"
$previewTagline = "A lightweight Windows desktop system monitor"

# The 256 px design, in pixels. The line runs flat at TraceTop, dips to the V's
# point at VBottom, and runs flat again. VHalfWidth is the distance from the
# centre to the V's upper corners, and LineHalfWidth to the line's ends.
$design = @{ TileInset = 8; TileRadius = 56; Stroke = 24; TraceTop = 82; VBottom = 180; VHalfWidth = 44; LineHalfWidth = 92 }

# The small sizes, placed on the pixel grid: a 2 px line whose flat parts cover
# whole rows of pixels, and a tile whose edges are pixel edges.
$smallDesigns = @{
    16 = @{ TileInset = 0; TileRadius = 3; Stroke = 2; TraceTop = 5; VBottom = 11; VHalfWidth = 3; LineHalfWidth = 5.5 }
    20 = @{ TileInset = 0; TileRadius = 4; Stroke = 2; TraceTop = 6; VBottom = 14; VHalfWidth = 3.5; LineHalfWidth = 7 }
    24 = @{ TileInset = 1; TileRadius = 5; Stroke = 2; TraceTop = 8; VBottom = 17; VHalfWidth = 4; LineHalfWidth = 8.5 }
}

function Get-IconGeometry
{
    param([Parameter(Mandatory)][int]$Size)

    if ($smallDesigns.ContainsKey($Size))
    {
        return $smallDesigns[$Size]
    }
    $scale = $Size / 256.0
    $geometry = @{}
    foreach ($key in $design.Keys)
    {
        $geometry[$key] = $design[$key] * $scale
    }
    # A tile edge inside a pixel would be blurred.
    $geometry.TileInset = [Math]::Round($design.TileInset * $scale)
    return $geometry
}

function New-RoundPen
{
    param(
        [Parameter(Mandatory)][System.Drawing.Color]$Color,
        [Parameter(Mandatory)][double]$Width
    )

    $pen = New-Object System.Drawing.Pen($Color, [single]$Width)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    return $pen
}

# Draws the line: flat at Top, from LineHalfWidth left of Centre to the V, down
# to the V's point at Bottom, up again, and flat to LineHalfWidth right of Centre.
function Add-Trace
{
    param(
        [Parameter(Mandatory)][System.Drawing.Graphics]$Graphics,
        [Parameter(Mandatory)][double]$Centre,
        [Parameter(Mandatory)][double]$Top,
        [Parameter(Mandatory)][double]$Bottom,
        [Parameter(Mandatory)][double]$VHalfWidth,
        [Parameter(Mandatory)][double]$LineHalfWidth,
        [Parameter(Mandatory)][double]$Stroke
    )

    $flatPen = New-RoundPen -Color ([System.Drawing.Color]::FromArgb([int][Math]::Round(255 * $flatOpacity), 255, 255, 255)) `
        -Width $Stroke
    $tracePen = New-RoundPen -Color ([System.Drawing.ColorTranslator]::FromHtml($traceColor)) -Width $Stroke
    try
    {
        $flatTop = [single]$Top
        $vLeft = [single]($Centre - $VHalfWidth)
        $vRight = [single]($Centre + $VHalfWidth)
        $Graphics.DrawLine($flatPen, [single]($Centre - $LineHalfWidth), $flatTop, $vLeft, $flatTop)
        $Graphics.DrawLine($flatPen, $vRight, $flatTop, [single]($Centre + $LineHalfWidth), $flatTop)
        # The V is drawn last, so that it covers the ends of the flat parts.
        $points = [System.Drawing.PointF[]]@(
            (New-Object System.Drawing.PointF($vLeft, $flatTop)),
            (New-Object System.Drawing.PointF([single]$Centre, [single]$Bottom)),
            (New-Object System.Drawing.PointF($vRight, $flatTop)))
        $Graphics.DrawLines($tracePen, $points)
    }
    finally
    {
        $tracePen.Dispose()
        $flatPen.Dispose()
    }
}

function New-IconBitmap
{
    param([Parameter(Mandatory)][int]$Size)

    $geometry = Get-IconGeometry -Size $Size
    $bitmap = New-Object System.Drawing.Bitmap($Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $tile = New-Object System.Drawing.Drawing2D.GraphicsPath
    $tileBrush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml($tileColor))
    try
    {
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        # Coordinates name pixel edges, as in SVG: a 2 px line at y = 5 covers
        # the rows of pixels from 4 to 6.
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $graphics.Clear([System.Drawing.Color]::Transparent)

        $near = [single]$geometry.TileInset
        $diameter = [single](2 * $geometry.TileRadius)
        $far = [single]($Size - $geometry.TileInset - $diameter)
        $tile.AddArc($near, $near, $diameter, $diameter, 180, 90)
        $tile.AddArc($far, $near, $diameter, $diameter, 270, 90)
        $tile.AddArc($far, $far, $diameter, $diameter, 0, 90)
        $tile.AddArc($near, $far, $diameter, $diameter, 90, 90)
        $tile.CloseFigure()
        $graphics.FillPath($tileBrush, $tile)

        Add-Trace -Graphics $graphics -Centre ($Size / 2.0) -Top $geometry.TraceTop -Bottom $geometry.VBottom `
            -VHalfWidth $geometry.VHalfWidth -LineHalfWidth $geometry.LineHalfWidth -Stroke $geometry.Stroke
    }
    finally
    {
        $tileBrush.Dispose()
        $tile.Dispose()
        $graphics.Dispose()
    }
    return $bitmap
}

# Returns the bitmap's pixels as bytes in the order B, G, R, A, from the top row down.
function Get-PixelBytes
{
    param([Parameter(Mandatory)][System.Drawing.Bitmap]$Bitmap)

    $rectangle = New-Object System.Drawing.Rectangle(0, 0, $Bitmap.Width, $Bitmap.Height)
    $data = $Bitmap.LockBits($rectangle, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try
    {
        $bytes = New-Object byte[] ($data.Stride * $Bitmap.Height)
        [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $bytes.Length)
    }
    finally
    {
        $Bitmap.UnlockBits($data)
    }
    return , $bytes
}

# Returns the image as an icon stores an uncompressed one: a BITMAPINFOHEADER,
# the colour rows, and the AND mask's rows, both from the bottom row up.
function Get-IconBitmapBytes
{
    param([Parameter(Mandatory)][System.Drawing.Bitmap]$Bitmap)

    $size = $Bitmap.Width
    $pixels = Get-PixelBytes -Bitmap $Bitmap
    $rowLength = $size * 4
    $maskRowLength = [int][Math]::Ceiling($size / 32.0) * 4

    $stream = New-Object System.IO.MemoryStream
    $writer = New-Object System.IO.BinaryWriter($stream)
    $writer.Write([uint32]40)
    $writer.Write([int32]$size)
    # The height counts the colour rows and the mask's rows.
    $writer.Write([int32](2 * $size))
    $writer.Write([uint16]1)
    $writer.Write([uint16]32)
    # BI_RGB: uncompressed.
    $writer.Write([uint32]0)
    $writer.Write([uint32](($rowLength + $maskRowLength) * $size))
    $writer.Write([int32]0)
    $writer.Write([int32]0)
    $writer.Write([uint32]0)
    $writer.Write([uint32]0)
    for ($row = $size - 1; $row -ge 0; $row--)
    {
        $writer.Write($pixels, $row * $rowLength, $rowLength)
    }
    # The mask marks the fully transparent pixels, for readers that ignore the alpha channel.
    for ($row = $size - 1; $row -ge 0; $row--)
    {
        $mask = New-Object byte[] $maskRowLength
        for ($column = 0; $column -lt $size; $column++)
        {
            if ($pixels[$row * $rowLength + $column * 4 + 3] -eq 0)
            {
                $index = [int][Math]::Floor($column / 8)
                $mask[$index] = [byte]($mask[$index] -bor (0x80 -shr ($column % 8)))
            }
        }
        $writer.Write($mask)
    }
    $writer.Flush()
    return , $stream.ToArray()
}

function New-SocialPreviewBitmap
{
    $width = 1280
    $height = 640
    $iconSize = 288
    $gap = 56
    $lineGap = 8

    $bitmap = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $icon = New-IconBitmap -Size $iconSize
    $pixel = [System.Drawing.GraphicsUnit]::Pixel
    $titleFont = New-Object System.Drawing.Font("Segoe UI Semibold", 120, [System.Drawing.FontStyle]::Regular, $pixel)
    $taglineFont = New-Object System.Drawing.Font("Segoe UI", 32, [System.Drawing.FontStyle]::Regular, $pixel)
    $titleBrush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml($previewTitleColor))
    $taglineBrush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml($previewTaglineColor))
    $format = [System.Drawing.StringFormat]::GenericTypographic
    try
    {
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
        $graphics.Clear([System.Drawing.ColorTranslator]::FromHtml($previewBackground))

        # The icon and the text are centred as one group, the two lines of text
        # as a block beside the icon.
        $origin = [System.Drawing.PointF]::Empty
        $titleSize = $graphics.MeasureString($previewTitle, $titleFont, $origin, $format)
        $taglineSize = $graphics.MeasureString($previewTagline, $taglineFont, $origin, $format)
        $textWidth = [Math]::Max($titleSize.Width, $taglineSize.Width)
        $left = ($width - ($iconSize + $gap + $textWidth)) / 2
        $graphics.DrawImage($icon, [single]$left, [single](($height - $iconSize) / 2), [single]$iconSize, [single]$iconSize)

        $textLeft = [single]($left + $iconSize + $gap)
        $top = ($height - ($titleSize.Height + $lineGap + $taglineSize.Height)) / 2
        $graphics.DrawString($previewTitle, $titleFont, $titleBrush, $textLeft, [single]$top, $format)
        $graphics.DrawString($previewTagline, $taglineFont, $taglineBrush, $textLeft,
            [single]($top + $titleSize.Height + $lineGap), $format)
    }
    finally
    {
        $format.Dispose()
        $taglineBrush.Dispose()
        $titleBrush.Dispose()
        $taglineFont.Dispose()
        $titleFont.Dispose()
        $icon.Dispose()
        $graphics.Dispose()
    }
    return $bitmap
}

# Returns the bounds of the text's glyphs, relative to the point that
# DrawString with GenericTypographic draws it at.
function Get-GlyphBounds
{
    param(
        [Parameter(Mandatory)][string]$Text,
        [Parameter(Mandatory)][System.Drawing.Font]$Font
    )

    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $format = [System.Drawing.StringFormat]::GenericTypographic
    try
    {
        $path.AddString($Text, $Font.FontFamily, [int]$Font.Style, $Font.Size, [System.Drawing.PointF]::Empty, $format)
        return $path.GetBounds()
    }
    finally
    {
        $format.Dispose()
        $path.Dispose()
    }
}

# The alternative social preview: the name with the icon's line in place of its
# V, and the tagline below it, centred, with no tile. The name is bold, whose
# stems are about as thick as the line at this size, so that the V does not
# outweigh the other letters.
function New-WordmarkPreviewBitmap
{
    $width = 1280
    $height = 640
    $titleSize = 220
    $taglineSize = 36
    $lineGap = 44

    $bitmap = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $pixel = [System.Drawing.GraphicsUnit]::Pixel
    $titleFont = New-Object System.Drawing.Font("Segoe UI", $titleSize, [System.Drawing.FontStyle]::Bold, $pixel)
    $taglineFont = New-Object System.Drawing.Font("Segoe UI", $taglineSize, [System.Drawing.FontStyle]::Regular, $pixel)
    $titleBrush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml($previewTitleColor))
    $taglineBrush = New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml($previewTaglineColor))
    $format = [System.Drawing.StringFormat]::GenericTypographic
    try
    {
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAlias
        $graphics.Clear([System.Drawing.ColorTranslator]::FromHtml($previewBackground))

        # The V spans the cap height, from its arms' round ends down to its
        # point's round join, and is centred where the font's V would be. The
        # rest of the name starts where it would after the font's V.
        $origin = [System.Drawing.PointF]::Empty
        $first = $previewTitle.Substring(0, 1)
        $rest = $previewTitle.Substring(1)
        $capBounds = Get-GlyphBounds -Text "H" -Font $titleFont
        $firstBounds = Get-GlyphBounds -Text $first -Font $titleFont
        $restBounds = Get-GlyphBounds -Text $rest -Font $titleFont
        $taglineBounds = Get-GlyphBounds -Text $previewTagline -Font $taglineFont
        $scale = $capBounds.Height / ($design.VBottom - $design.TraceTop + $design.Stroke)
        $stroke = $design.Stroke * $scale
        $lineHalfWidth = $design.LineHalfWidth * $scale
        $vCentre = $firstBounds.Left + $firstBounds.Width / 2
        $restLeft = $graphics.MeasureString($first, $titleFont, $origin, $format).Width

        # The name, from the left end of the line to its last letter and from
        # its tallest letter to the baseline, and the tagline below it are
        # centred as one block.
        $nameLeft = $vCentre - $lineHalfWidth - $stroke / 2
        $nameRight = $restLeft + $restBounds.Right
        $nameTop = [Math]::Min($capBounds.Top, $restBounds.Top)
        $blockHeight = $capBounds.Bottom - $nameTop + $lineGap + $taglineBounds.Bottom
        $x = ($width - ($nameRight - $nameLeft)) / 2 - $nameLeft
        $y = ($height - $blockHeight) / 2 - $nameTop

        Add-Trace -Graphics $graphics -Centre ($x + $vCentre) -Top ($y + $capBounds.Top + $stroke / 2) `
            -Bottom ($y + $capBounds.Bottom - $stroke / 2) -VHalfWidth ($design.VHalfWidth * $scale) `
            -LineHalfWidth $lineHalfWidth -Stroke $stroke
        $graphics.DrawString($rest, $titleFont, $titleBrush, [single]($x + $restLeft), [single]$y, $format)
        $taglineWidth = $graphics.MeasureString($previewTagline, $taglineFont, $origin, $format).Width
        $graphics.DrawString($previewTagline, $taglineFont, $taglineBrush, [single](($width - $taglineWidth) / 2),
            [single]($y + $capBounds.Bottom + $lineGap), $format)
    }
    finally
    {
        $format.Dispose()
        $taglineBrush.Dispose()
        $titleBrush.Dispose()
        $taglineFont.Dispose()
        $titleFont.Dispose()
        $graphics.Dispose()
    }
    return $bitmap
}

function Get-PngBytes
{
    param([Parameter(Mandatory)][System.Drawing.Bitmap]$Bitmap)

    $stream = New-Object System.IO.MemoryStream
    $Bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    return , $stream.ToArray()
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
if ($PngDirectory -ne "")
{
    $PngDirectory = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($PngDirectory)
    New-Item -ItemType Directory -Force -Path $PngDirectory | Out-Null
}

$images = @()
foreach ($size in $sizes)
{
    $bitmap = New-IconBitmap -Size $size
    try
    {
        if ($size -eq 256)
        {
            $bytes = Get-PngBytes -Bitmap $bitmap
            [System.IO.File]::WriteAllBytes((Join-Path $OutputDirectory "verdandi_256.png"), $bytes)
        }
        else
        {
            $bytes = Get-IconBitmapBytes -Bitmap $bitmap
        }
        $images += , @{ Size = $size; Bytes = $bytes }
        if ($PngDirectory -ne "")
        {
            $bitmap.Save((Join-Path $PngDirectory "verdandi_$size.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        }
    }
    finally
    {
        $bitmap.Dispose()
    }
}

# ICONDIR, then an ICONDIRENTRY per image, then the images. A width or height of
# 0 stands for 256.
$stream = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($stream)
$writer.Write([uint16]0)
$writer.Write([uint16]1)
$writer.Write([uint16]$images.Count)
$offset = 6 + 16 * $images.Count
foreach ($image in $images)
{
    $writer.Write([byte]($image.Size % 256))
    $writer.Write([byte]($image.Size % 256))
    $writer.Write([byte]0)
    $writer.Write([byte]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]32)
    $writer.Write([uint32]$image.Bytes.Length)
    $writer.Write([uint32]$offset)
    $offset += $image.Bytes.Length
}
foreach ($image in $images)
{
    $writer.Write($image.Bytes)
}
$writer.Flush()
$iconPath = Join-Path $OutputDirectory "verdandi.ico"
[System.IO.File]::WriteAllBytes($iconPath, $stream.ToArray())

$centre = 128
$tileExtent = 256 - 2 * $design.TileInset
$lineStart = $centre - $design.LineHalfWidth
$lineEnd = $centre + $design.LineHalfWidth
$vLeft = $centre - $design.VHalfWidth
$vRight = $centre + $design.VHalfWidth
$svg = @"
<svg xmlns="http://www.w3.org/2000/svg" width="256" height="256" viewBox="0 0 256 256">
  <title>Verdandi</title>
  <rect x="$($design.TileInset)" y="$($design.TileInset)" width="$tileExtent" height="$tileExtent" rx="$($design.TileRadius)" fill="$tileColor"/>
  <path d="M$lineStart $($design.TraceTop) H$vLeft M$vRight $($design.TraceTop) H$lineEnd" fill="none" stroke="#FFFFFF" stroke-opacity="$flatOpacity" stroke-width="$($design.Stroke)" stroke-linecap="round"/>
  <path d="M$vLeft $($design.TraceTop) L$centre $($design.VBottom) L$vRight $($design.TraceTop)" fill="none" stroke="$traceColor" stroke-width="$($design.Stroke)" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
"@
$svgPath = Join-Path $OutputDirectory "verdandi.svg"
[System.IO.File]::WriteAllText($svgPath, $svg.Replace("`r`n", "`n") + "`n", (New-Object System.Text.UTF8Encoding($false)))

$preview = New-SocialPreviewBitmap
try
{
    $previewPath = Join-Path $OutputDirectory "social_preview.png"
    [System.IO.File]::WriteAllBytes($previewPath, (Get-PngBytes -Bitmap $preview))
}
finally
{
    $preview.Dispose()
}
$wordmarkPreview = New-WordmarkPreviewBitmap
try
{
    [System.IO.File]::WriteAllBytes((Join-Path $OutputDirectory "social_preview_wordmark.png"),
        (Get-PngBytes -Bitmap $wordmarkPreview))
}
finally
{
    $wordmarkPreview.Dispose()
}

Write-Host ("Wrote $iconPath ($($sizes -join ', ') px), $svgPath, verdandi_256.png, social_preview.png and " +
    "social_preview_wordmark.png.")
