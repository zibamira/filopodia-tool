#ifndef VSFILOPODIAPICKTHRESHOLD_H
#define VSFILOPODIAPICKTHRESHOLD_H

#include <vsvolren/internal/VsPickThreshold.h>
#include <mclib/McVec3.h>
#include <vector>

class VsVolren;
class HxNeuronEditorSubApp;


// Selects point on picking ray that is the first intensity maximum above an intensity threshold
// after the accumulated opacity passed a threshold
class VsFilopodiaPickThreshold : public VsPickThreshold {
public:
    VsFilopodiaPickThreshold(VsVolren* v);
    ~VsFilopodiaPickThreshold();

    virtual bool reached(const McVec3f& vec);
    virtual McVec3f result(bool& resultFound);
    virtual float intensity() const;
    virtual bool needAllSamples();

private:
    float             mMaxIntensity;
    std::vector<McVec3f> mPositions;
    std::vector<float>   mIntensities;
    std::vector<float>   mAlphas;
};


// Uses the wysiwyp algorithm (Wiebel et al, 2012) to detect picking ray segment with largest
// jump in accumulated opacity. Extension: the picked point is the point with the maximal intensity
// in this ray segment.
class VsWysiwypIntensityThreshold : public VsPickThreshold {
public:
    VsWysiwypIntensityThreshold(VsVolren* v, HxNeuronEditorSubApp* editor);

    virtual bool reached(const McVec3f& vec);
    virtual McVec3f result(bool &resultFound);
    virtual float intensity() const;
    virtual bool needAllSamples();

private:
    HxNeuronEditorSubApp* mEditor;
    std::vector<McVec3f> mPositions;
    std::vector<float>   mIntensities;
    std::vector<float>   mAlphas;
};


class VsFilopodiaFirstMaximumPickThreshold : public VsPickThreshold {
public:
    VsFilopodiaFirstMaximumPickThreshold(VsVolren* v);

    virtual bool reached(const McVec3f& vec);
    virtual McVec3f result(bool& resultFound);
    virtual float intensity() const;
    virtual bool needAllSamples();

private:
    float                 mMaxIntensity;
    std::vector<McVec3f>     mPositions;
    std::vector<float>       mIntensities;
};

#endif // VSFILOPODIAPICKTHRESHOLD_H
