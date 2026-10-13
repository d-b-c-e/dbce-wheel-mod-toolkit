using Windows.Graphics.Imaging;
using Windows.Storage.Streams;

internal sealed record EncodedPng(byte[] Bytes, int Width, int Height);

internal static class CaptureEncoding
{
    internal static async Task<EncodedPng> Png(SoftwareBitmap bitmap, int requestedWidth, CaptureCrop? crop)
    {
        if (crop != null && requestedWidth != 0) throw new ArgumentException("crop and resize cannot be combined");
        crop?.RequireFits(bitmap.PixelWidth, bitmap.PixelHeight);
        using var stream = new InMemoryRandomAccessStream();
        var encoder = await BitmapEncoder.CreateAsync(BitmapEncoder.PngEncoderId, stream);
        encoder.SetSoftwareBitmap(bitmap);
        int width = bitmap.PixelWidth, height = bitmap.PixelHeight;
        if (crop is { } bounds)
        {
            encoder.BitmapTransform.Bounds = new BitmapBounds
            {
                X = (uint)bounds.X, Y = (uint)bounds.Y, Width = (uint)bounds.Width, Height = (uint)bounds.Height
            };
            width = bounds.Width; height = bounds.Height;
        }
        else if (requestedWidth > 0 && requestedWidth < width)
        {
            height = Math.Max(1, (int)(height * (long)requestedWidth / width)); width = requestedWidth;
            encoder.BitmapTransform.ScaledWidth = (uint)width;
            encoder.BitmapTransform.ScaledHeight = (uint)height;
            encoder.BitmapTransform.InterpolationMode = BitmapInterpolationMode.Fant;
        }
        await encoder.FlushAsync();
        var bytes = new byte[stream.Size];
        using (var reader = new DataReader(stream.GetInputStreamAt(0)))
        {
            await reader.LoadAsync((uint)stream.Size);
            reader.ReadBytes(bytes);
        }
        return new(bytes, width, height);
    }
}
