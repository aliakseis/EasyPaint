#include "customeffect.h"

static QRgb convolutePixel(const QImage& image, int x, int y, const QList<QVariant>& kernelMatrix)
{
    int kernelSize = sqrt(kernelMatrix.size());

    double total = 0;
    double red = 0;
    double green = 0;
    double blue = 0;
    // TODO: some optimization can be made (maybe use OpenCV in future
    for (int r = -kernelSize / 2; r <= kernelSize / 2; ++r)
    {
        for (int c = -kernelSize / 2; c <= kernelSize / 2; ++c)
        {
            int kernelValue = kernelMatrix[(kernelSize / 2 + r) * kernelSize + (kernelSize / 2 + c)].toDouble();
            total += kernelValue;
            red += qRed(image.pixel(x + c, y + r)) * kernelValue;
            green += qGreen(image.pixel(x + c, y + r)) * kernelValue;
            blue += qBlue(image.pixel(x + c, y + r)) * kernelValue;
        }
    }

    if (total == 0)
        return qRgb(qBound(0, qRound(red), 255), qBound(0, qRound(green), 255), qBound(0, qRound(blue), 255));

    return qRgb(qBound(0, qRound(red / total), 255), qBound(0, qRound(green / total), 255), qBound(0, qRound(blue / total), 255));
}


void CustomEffect::convertImage(const QImage* source, QImage& mImage, const QVariantList& matrix, std::weak_ptr<EffectRunCallback> /*callback*/)
{
    QImage copy(*source);

    for (int i = 2; i < copy.height() - 2; ++i)
    {
        for (int j = 2; j < copy.width() - 2; ++j)
        {
            copy.setPixel(j, i, convolutePixel(*source, j, i, matrix));
        }
    }

    mImage = copy;
}
