#ifndef SET_UNION_DATA_STRUCTURE_H
#define SET_UNION_DATA_STRUCTURE_H

#include "api.h"

#include <mclib/McPrimType.h>

#include <vector>

class HXCONTOURTREE_API SetUnionDataStructure
{
public:
    void setNumElements(const mclong numElements);
    void setSetIdOfElement(const mclong elemId, const mclong setId);
    void mergeSetsOfElements(const mculong elem1Id, const mculong elem2Id);
    mculong findSetId(const mculong elemId);
    mclong findSetIdFailSafe(const mculong elemId);
    mculong getNumElements() const;

private:
    void mergeSets(const mculong setId, const mculong newSetId);

private:
    std::vector< mclong > m_setIds;
};

#endif
