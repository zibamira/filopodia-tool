#include <hxqt/QxStringUtils.h>

#include "QxVariantConversions.h"
#include <mclib/McException.h>

QVariant
qxvariantconversions::pointArrayToVariant(const std::vector<McVec3f>& points)
{
    QVariantList coordList;
    for (int i = 0; i < points.size(); ++i)
    {
        coordList.append(QVariant(double(points[i][0])));
        coordList.append(QVariant(double(points[i][1])));
        coordList.append(QVariant(double(points[i][2])));
    }
    return QVariant(coordList);
}

QVariant
qxvariantconversions::floatArrayToVariant(const McDArray<float>& values)
{
    QVariantList valueList;
    for (int i = 0; i < values.size(); ++i)
    {
        valueList.append(QVariant(double(values[i])));
    }
    return QVariant(valueList);
}

McDArray<float>
qxvariantconversions::variantToFloatArray(const QVariant& v)
{
    if (!v.canConvert<QVariantList>())
    {
        throw McException(QString("Cannot create float array: not a list"));
    }
    QVariantList valueList = v.toList();
    McDArray<float> result(valueList.size());
    for (int i = 0; i < valueList.size(); ++i)
    {
        bool ok;
        result[i] = float(valueList[i].toDouble(&ok));
        if (!ok)
        {
            throw McException(QString("Cannot create float array: cannot read value %1 double value").arg(i));
        }
    }
    return result;
}

std::vector<McVec3f>
qxvariantconversions::variantToPointArray(const QVariant& v)
{
    std::vector<McVec3f> points;
    if (!v.canConvert<QVariantList>())
    {
        throw McException(QString("Cannot create point array: not a list"));
    }
    QVariantList coordList = v.toList();
    if (coordList.size() % 3 != 0)
    {
        throw McException(QString("Cannot create point array: number of coordinate values must be multiple of 3"));
    }
    const int numPoints = coordList.size() / 3;
    points.resize(numPoints);
    for (int i = 0; i < numPoints; ++i)
    {
        for (int j = 0; j <= 2; ++j)
        {
            const QVariant coordVariant = coordList[3 * i + j];
            bool ok;
            points[i][j] = float(coordVariant.toDouble(&ok));
            if (!ok)
            {
                throw McException(QString("Cannot create point array: cannot read edgePoint [%1][%2] double value").arg(i).arg(j));
            }
        }
    }
    return points;
}

QVariant
qxvariantconversions::pointToVariant(const McVec3f& point)
{
    QVariantList coordList;
    coordList.append(QVariant(double(point[0])));
    coordList.append(QVariant(double(point[1])));
    coordList.append(QVariant(double(point[2])));
    return QVariant(coordList);
}

McVec3f
qxvariantconversions::variantToPoint(const QVariant& v)
{
    if (!v.canConvert<QVariantList>())
    {
        throw McException(QString("Cannot create point: not a list"));
    }
    QVariantList coordList = v.toList();
    if (coordList.size() != 3)
    {
        throw McException(QString("Cannot create point: number of coordinate values must be 3"));
    }

    McVec3f point;
    for (int j = 0; j <= 2; ++j)
    {
        const QVariant coordVariant = coordList[j];
        bool ok;
        point[j] = float(coordVariant.toDouble(&ok));
        if (!ok)
        {
            throw McException(QString("Cannot create point: point coord [%1] of type %2 cannot be converted to double")
                                  .arg(j)
                                  .arg(toQString(coordVariant.typeName())));
        }
    }
    return point;
}

QVariant
qxvariantconversions::intToVariant(const int value)
{
    return QVariant(value);
}

int
qxvariantconversions::variantToInt(const QVariant& v)
{
    if (!v.canConvert<int>())
    {
        throw McException(QString("Cannot create integer: variant is a %1").arg(toQString(v.typeName())));
    }
    return v.toInt();
}

QVariant
qxvariantconversions::stringToVariant(const QString& str)
{
    return QVariant(str);
}

QString
qxvariantconversions::variantToString(const QVariant& v)
{
    if (!v.canConvert<QString>())
    {
        throw McException(QString("Cannot create string: variant is a %1").arg(toQString(v.typeName())));
    }
    return v.toString();
}

QVariant
qxvariantconversions::intArrayToVariant(const McDArray<int>& values)
{
    QVariantList valueList;
    for (int i = 0; i < values.size(); ++i)
    {
        valueList.append(QVariant(values[i]));
    }
    return QVariant(valueList);
}

McDArray<int>
qxvariantconversions::variantToIntArray(const QVariant& v)
{
    if (!v.canConvert<QVariantList>())
    {
        throw McException(QString("Cannot create int array: not a list"));
    }
    QVariantList valueList = v.toList();
    McDArray<int> result(valueList.size());
    for (int i = 0; i < valueList.size(); ++i)
    {
        bool ok;
        result[i] = float(valueList[i].toInt(&ok));
        if (!ok)
        {
            throw McException(QString("Cannot create int array: cannot read value %1 int value").arg(i));
        }
    }
    return result;
}
