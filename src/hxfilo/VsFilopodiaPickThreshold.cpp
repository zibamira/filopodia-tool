#include "VsFilopodiaPickThreshold.h"
#include <hxneuroneditor/internal/HxNeuronEditorSubApp.h>
#include <vsvolren/internal/VsColorTable.h>
#include <vsvolren/internal/VsVolren.h>
#include <vsvolren/internal/VsData.h>
#include <mclib/internal/McAssert.h>
#include <QList>
#include <QDebug>
#include <QtAlgorithms>

VsFilopodiaPickThreshold::VsFilopodiaPickThreshold(VsVolren* v)
    : VsPickThreshold(v, 0)
    , mMaxIntensity(0)
{
}


VsFilopodiaPickThreshold::~VsFilopodiaPickThreshold()
{
}


bool VsFilopodiaPickThreshold::needAllSamples() {
    return true;
}


float VsFilopodiaPickThreshold::intensity() const {
    return mMaxIntensity;
}


bool VsFilopodiaPickThreshold::reached(const McVec3f& vec) {
    VsColorMap* cm = mVolren->transferFunction(0, mVolIdx);
    if (!cm) {
        return false;
    }

    float intensity = 0.0f;

    mPositions.push_back(vec);

    if (mLabels)
    {
        mLabels->sample(vec, 1, &intensity);
        if (mVolren->labelClipped((int)(intensity + 0.5f))) {
            mAlphas.push_back(0.0f);
            return false;
        }
    }

    mVoldata->sample(vec, 1, &intensity);
    mIntensities.push_back(intensity);

    const float min = 0.0f;
    const float max = 255.0f;
    const float range = max - min;
    const float u = range == 0 ? 0 : (intensity - min) / range;

    float alpha = cm->alpha1(u);

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    mAlphas.push_back(alpha);

    mcassert(mPositions.size() == mAlphas.size());
    mcassert(mPositions.size() == mIntensities.size());

    return false;
}


McVec3f VsFilopodiaPickThreshold::result(bool &resultFound) {
    resultFound = false;

    if (mPositions.size() < 5) {
        return mResult;
    }

    int firstIndexPassingAlphaThreshold = -1;

    const float ACCUMULATED_OPACITY_THRESHOLD = 0.5;
    const float INTENSITY_THRESHOLD = 20.f;

    std::vector<float> accumulatedAlphas(mAlphas.size());
    float opacity = 0.0f;
    for (int i=0; i<mAlphas.size(); ++i) {
        opacity = opacity + (1.0f - opacity) * mAlphas[i];
        accumulatedAlphas[i] = opacity;
        if (opacity > ACCUMULATED_OPACITY_THRESHOLD && firstIndexPassingAlphaThreshold == -1) {
            firstIndexPassingAlphaThreshold = i;
        }
    }

    std::vector<float> smoothedIntensities(mIntensities);
    for (int i=2; i<mIntensities.size()-2; ++i) {
        smoothedIntensities[i] = (mIntensities[i-2] + mIntensities[i-1] +
                mIntensities[i] +
                mIntensities[i+1] + mIntensities[i+2])/5.0f;
    }

    if (firstIndexPassingAlphaThreshold == -1) {
        return mResult;
    }

    for (int i=0; i<mPositions.size(); ++i) {
        printf("%d\t%f\t%f\t%f\n", i, accumulatedAlphas[i], mIntensities[i]/255, smoothedIntensities[i]/255);
    }

    float smoothedMaxIntensity = 0.0f;
    int indexWithMaximumIntensity = -1;
    for (int i=firstIndexPassingAlphaThreshold; i<smoothedIntensities.size(); ++i) {
        if (smoothedIntensities[i] > INTENSITY_THRESHOLD && smoothedIntensities[i] > smoothedMaxIntensity) {
            indexWithMaximumIntensity = i;
            smoothedMaxIntensity = smoothedIntensities[i];
        }
        else if (smoothedIntensities[i] < smoothedMaxIntensity) {
            mResult = mPositions[indexWithMaximumIntensity];
            mMaxIntensity = mIntensities[indexWithMaximumIntensity];
            printf("RESULT INDEX %d\n", indexWithMaximumIntensity);
            resultFound = true;
            return mResult;
        }
    }

    if (indexWithMaximumIntensity == -1) {
        return mResult;
    }

    return mResult;
}


struct Interval {
    int minIdx;
    int maxIdx;
    float jumpSize;
};


bool IntervalMoreThan(const Interval &i1, const Interval &i2)
{
    return i1.jumpSize > i2.jumpSize;
}


typedef QList<Interval> IntervalList;
enum Sign { NEGATIVE=0, ZERO=1, POSITIVE=2 };


std::vector<Sign> computeSigns(const std::vector<float>& secondDeriv) {
    std::vector<Sign> signs(secondDeriv.size());

    const float eps = 0.01f;
    for (int i=0; i<secondDeriv.size(); ++i) {
        if (secondDeriv[i] > eps) {
            signs[i] = POSITIVE;
        }
        else if (secondDeriv[i] < -eps) {
            signs[i] = NEGATIVE;
        }
        else {
            signs[i] = ZERO;
        }
    }
    return signs;
}


IntervalList computeIntervalList(const std::vector<float>& accumAlpha,
                                 const std::vector<Sign>& signs)
{
    int currentIntervalStart = -1;
    int currentIntervalEnd = -1;;

    IntervalList list;

    for (int i=1; i<signs.size(); ++i) {
        Sign previous = signs[i-1];
        Sign current = signs[i];

        if (currentIntervalStart == -1) {
            // No current interval: do nothing unless start is found
            if ((previous == ZERO && current == POSITIVE) || (previous == NEGATIVE && current == POSITIVE)) {
                currentIntervalStart = i;
            }
        }
        else {
            // Close current interval: do nothing unless end is found
            if ((previous == NEGATIVE && current == ZERO) || (previous == NEGATIVE && current == POSITIVE)) {
                currentIntervalEnd = i;

                Interval interval;
                interval.minIdx = currentIntervalStart;
                interval.maxIdx = currentIntervalEnd;
                interval.jumpSize = (accumAlpha[interval.maxIdx] - accumAlpha[interval.minIdx]);
                list.append(interval);

                if (previous == NEGATIVE && current == POSITIVE) {
                    currentIntervalStart = i;
                }
                else {
                    currentIntervalStart = -1;
                }
                currentIntervalEnd = -1;
            }
        }
    }

    // Close current interval
    if (currentIntervalStart != -1 && currentIntervalStart != signs.size()-1) {
        Interval interval;
        interval.minIdx = currentIntervalStart;
        interval.maxIdx = signs.size() - 1;
        interval.jumpSize = (accumAlpha[interval.maxIdx] - accumAlpha[interval.minIdx]);
        list.append(interval);
    }

    for (int i=0; i<list.size(); ++i) {
        qDebug() << "Interval" << i << list[i].minIdx << list[i].maxIdx << list[i].jumpSize;
    }

    return list;
}


std::vector<float> computeAccumulatedAlpha(const std::vector<float>& alphas) {
    std::vector<float> result(alphas.size());
    float opacity = 0.f;
    for (int i=0; i<alphas.size(); ++i) {
        opacity = opacity + (1.0f - opacity) * alphas[i];
        result[i] = opacity;
    }
    return result;
}


std::vector<float> computeSecondDerivative(const std::vector<float>& values) {
    std::vector<float> result(values.size());
    std::fill(result.begin(),result.end(),0.0f);
    for (int i=1; i<values.size()-1; ++i )
    {
        result[i] = values[i-1] - 2*values[i] + values[i+1];
    }
    return result;
}


VsWysiwypIntensityThreshold::VsWysiwypIntensityThreshold(VsVolren* v, HxNeuronEditorSubApp* editor)
    : VsPickThreshold(v, 0), mEditor(editor)
{
}


float VsWysiwypIntensityThreshold::intensity() const {
    float intensity;
    mVoldata->sample(mResult, 1, &intensity);
    return intensity;
}


bool VsWysiwypIntensityThreshold::needAllSamples() {
    return true;
}


bool VsWysiwypIntensityThreshold::reached(const McVec3f& vec)
{
    VsColorMap* cm = mVolren->transferFunction(0, mVolIdx);
    if (!cm) {
        return false;
    }

    float intensity = 0.0f;

    mPositions.push_back(vec);

    if (mLabels)
    {
        mLabels->sample(vec, 1, &intensity);
        if (mVolren->labelClipped((int)(intensity + 0.5f))) {
            mAlphas.push_back(0.0f);
            mIntensities.push_back(0.0f);
            return false;
        }
    }

    mVoldata->sample(vec, 1, &intensity);
    mIntensities.push_back(intensity);

    const float min = mVolren->dataWindowMin(0, mVolIdx);
    const float max = mVolren->dataWindowMax(0, mVolIdx);
    const float range = max - min;
    const float u = (fabs(range) < 0.001f) ? 0.f : (intensity - min) / range;
    qDebug() << "WINDOW" << min << max << u;

    float alpha = cm->alpha1(u);

    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    mAlphas.push_back(alpha);

    mcassert(mPositions.size() == mAlphas.size());
    mcassert(mPositions.size() != mIntensities.size());

    return false;
}


int getIntervalId(int idx, const IntervalList& intervals) {
    for (int i=0; i<intervals.size(); ++i) {
        if (idx >= intervals[i].minIdx && idx <= intervals[i].maxIdx) {
            return i+1;
        }
    }
    return 0;
}


McVec3f VsWysiwypIntensityThreshold::result(bool& resultFound) {
    resultFound = false;

    if (mPositions.size() < 3) {
        return mResult;
    }

    const std::vector<float> alphaAccum = computeAccumulatedAlpha(mAlphas);
    const std::vector<float> alphaAccum2ndDeriv = computeSecondDerivative(alphaAccum);
    const std::vector<Sign> signs = computeSigns(alphaAccum2ndDeriv);
    IntervalList intervalList = computeIntervalList(alphaAccum, signs);


    QVector<QString> signStr(3);
    signStr[NEGATIVE] = "NEG";
    signStr[ZERO] = "ZERO";
    signStr[POSITIVE] = "POS";

    for (int i=0; i<mPositions.size(); ++i) {
        const int intervalId = getIntervalId(i, intervalList);
        const float intervalValue = intervalList.size() == 0 ? 0.0f : float(intervalId)/intervalList.size();
        if (intervalValue > 0.0f) {
            qDebug() << i << mAlphas[i] << alphaAccum[i] << alphaAccum2ndDeriv[i] << mIntensities[i]/255. << intervalValue << mIntensities[i] << signStr[signs[i]];
        }
        else {
            qDebug() << i << mAlphas[i] << alphaAccum[i] << alphaAccum2ndDeriv[i] << mIntensities[i]/255. << "\t" << mIntensities[i] << signStr[signs[i]];
        }
    }


    qSort(intervalList.begin(), intervalList.end(), IntervalMoreThan);

    for (int i=0; i<intervalList.size(); ++i) {
        float maxIntensity = -1.0f;
        int pickIdx = -1;
        const Interval& maxJumpInterval = intervalList[i];
        int numMaximumSamples = 0;

        for (int posIdx=maxJumpInterval.minIdx; posIdx<=maxJumpInterval.maxIdx; ++posIdx) {
            if (mIntensities[posIdx] > maxIntensity) {
                maxIntensity = mIntensities[posIdx];
                pickIdx = posIdx;
                numMaximumSamples = 1;
            }
            else if (fabs(maxIntensity -mIntensities[posIdx]) < 0.0001) {
                ++numMaximumSamples;
            }
        }

        if (mEditor->isValueInsideAllowedIntensityRange(maxIntensity)) {
            const int offset = ((numMaximumSamples-1)/2);
            pickIdx += offset;
            mResult = mPositions[pickIdx];
            resultFound = true;

            qDebug() << "Result index:" << pickIdx;
            qDebug() << "Num. max. samples:" << numMaximumSamples;
            qDebug() << "Offset:" << offset;
            qDebug() << "Intensity:" << maxIntensity;
            qDebug() << "Interval number:" << i;
            return mResult;
        }
        else {
            qDebug() << "Not using interval" << i << "jump size:" << maxJumpInterval.jumpSize << "max. intensity:" << maxIntensity;
        }
    }

    return mResult;
}


VsFilopodiaFirstMaximumPickThreshold::VsFilopodiaFirstMaximumPickThreshold(VsVolren *v)
    : VsPickThreshold(v, 0)
{
}


std::vector<float> smoothIntensities(const std::vector<float>& intensities, const int numNeighbors) {
    std::vector<float> smoothedIntensities(intensities);
    for (int i=numNeighbors; i<intensities.size()-numNeighbors; ++i) {
        float sum = 0.0f;
        int numValues = 0;
        for (int n=-numNeighbors; n<=numNeighbors; ++n) {
            sum += intensities[i+n];
            ++numValues;
        }
        smoothedIntensities[i] = sum/float(numValues);
    }
    return smoothedIntensities;

}

bool VsFilopodiaFirstMaximumPickThreshold::reached(const McVec3f &vec)
{
    float intensity = 0.0f;

    mPositions.push_back(vec);

    if (mLabels)
    {
        mLabels->sample(vec, 1, &intensity);
        if (mVolren->labelClipped((int)(intensity + 0.5f))) {
            mIntensities.push_back(0.0f);
            return false;
        }
    }

    mVoldata->sample(vec, 1, &intensity);
    mIntensities.push_back(intensity);
    return false;
}


McVec3f VsFilopodiaFirstMaximumPickThreshold::result(bool &resultFound)
{
    qDebug() << "\nPICK POINT IN 3D";
    mResult = McVec3f(0.0f, 0.0f, 0.0f);
    resultFound = false;

    // For bright filaments:
    // dataWindowMin = autoSkeletonThreshold = windowLevelMin + (windowLevelMax - windowLevelMin) * 0.1f;
    // validIntensityRange = [windowLevelMin + (windowLevelMax - windowLevelMin) * 0.1f, 255]
    // Thus, the threshold 'min' below ensures that the found intensity is in the valid intensity range.
    // Therefore, we don't need to call HxNeuronEditorSubApp->isValueInsideAllowedIntensityRange().
    const float min = mVolren->dataWindowMin(0, mVolIdx);
    int pickIdx = 0;
    while (pickIdx < mPositions.size() && mIntensities[pickIdx] < min) {
        ++pickIdx;
    }

    if (pickIdx == mPositions.size()) {
        for (int i=0; i<mPositions.size(); ++i) {
            qDebug() << "\t" << i << mIntensities[i];
        }
        qDebug() << "\tNO RESULT";
        resultFound = false;
        return mResult;
    }

    const std::vector<float> smoothedIntensities = smoothIntensities(mIntensities, 1);

    int maxIdx = pickIdx;
    float maxSmoothedIntensity = smoothedIntensities[maxIdx];
    while (maxIdx < smoothedIntensities.size() -1 && smoothedIntensities[maxIdx+1] > maxSmoothedIntensity) {
        maxIdx += 1;
        maxSmoothedIntensity = smoothedIntensities[maxIdx];
    }

    mResult = mPositions[maxIdx];
    resultFound = true;

    qDebug() << "\tResult index:" << maxIdx;
    qDebug() << "\tIntensity:" << mIntensities[maxIdx] << "Minimum intensity:" << min;

    return mResult;
}


float VsFilopodiaFirstMaximumPickThreshold::intensity() const
{
    float intensity;
    mVoldata->sample(mResult, 1, &intensity);
    return intensity;
}


bool VsFilopodiaFirstMaximumPickThreshold::needAllSamples()
{
    return true;
}


