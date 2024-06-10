#ifndef QXVARIANTCONVERSIONS_H
#define QXVARIANTCONVERSIONS_H

#include <hxspatialgraph/api.h>
#include <QVariant>
#include <mclib/McDArray.h>
#include <mclib/McVec3.h>
#include <vector>

namespace qxvariantconversions
{
    HXSPATIALGRAPH_API QVariant pointArrayToVariant(const std::vector<McVec3f>& points);
    HXSPATIALGRAPH_API std::vector<McVec3f> variantToPointArray(const QVariant& v);

    HXSPATIALGRAPH_API QVariant pointToVariant(const McVec3f& point);
    HXSPATIALGRAPH_API McVec3f variantToPoint(const QVariant& v);

    HXSPATIALGRAPH_API QVariant intToVariant(const int value);
    HXSPATIALGRAPH_API int variantToInt(const QVariant& v);

    HXSPATIALGRAPH_API QVariant stringToVariant(const QString& str);
    HXSPATIALGRAPH_API QString variantToString(const QVariant& v);

    HXSPATIALGRAPH_API QVariant floatArrayToVariant(const McDArray<float>& values);
    HXSPATIALGRAPH_API McDArray<float> variantToFloatArray(const QVariant& v);

    HXSPATIALGRAPH_API QVariant intArrayToVariant(const McDArray<int>& values);
    HXSPATIALGRAPH_API McDArray<int> variantToIntArray(const QVariant& v);
}

#endif // QXVARIANTCONVERSIONS_H
