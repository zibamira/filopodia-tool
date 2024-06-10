#include <hxcontourtree/HxMergeTreeSegmentation.h>

#include <hxfield/HxUniformScalarField3.h>
#include <hxfield/HxUniformVectorField3.h>
#include <hxfield/HxLabelLattice3.h>

#include <hxfield/HxLattice3.h>
#include <hxfield/HxUniformLabelField3.h>




HX_INIT_CLASS(HxMergeTreeSegmentation, HxCompModule)

HxMergeTreeSegmentation::HxMergeTreeSegmentation()
    : HxCompModule{HxUniformScalarField3::getClassTypeId()}
    , portBackwardDeformation{this, "backwardDeformation", tr("Backward Deform."), HxUniformVectorField3::getClassTypeId()}
    , portForwardDeformation{this, "forwardDeformation", tr("Forward Deform."), HxUniformVectorField3::getClassTypeId()}
    , portInterpretation{this, "interpretation", tr("Interpretation"), static_cast<int>(Interpretation::NUM_INTERPRETATIONS)}
    , portConnectivity{this, "connectivity", tr("Connectivity"), static_cast<int>(Connectivity::NUM_CONNECTIVITIES)}
    , portMergeTree{this, "mergeTree", tr("Merge Tree"), static_cast<int>(MergeMode::NUM_MERGEMODI)}
    , portSortNeighbours{this, "sortNeighbours", tr("Sort Neighbours"), static_cast<int>(SortNeighbours::NUM_NEIGHBOURMODI)}
    , portUseGlobalRange{this, "useGlobalRange", tr("Use Global Range")}
    , portGlobalRange{this, "globalRange", tr("Global Range"), 2}
    , portRange{this, "range", tr("Range"), 2}
    , portPersistenceMode{this, "persistenceMode", tr("Persistence Mode"), static_cast<int>(PersistenceMode::NUM_PERSISTANCEMODI)}
    , portPersistence{this, "persistence", tr("Persistence")}
    , portSegmentationMode{this, "segmentationMode", tr("Segmentation mode"), static_cast<int>(SegmentationMode::NUM_SEGMENTATIONMODI)}
    , portConsistentExtremaLabels{this, "consistentExtremaLabels", tr("Consistent labels")}
    , portNestedCoresWithSideBranches{this, "nestedCoresWithSideBranches", tr("Side branches")}
    , portDoIt{this, "doIt", tr("Apply")}
    , m_srcLat{nullptr}
    , m_dataMin{0.0f}
    , m_dataMax{0.0f}
    , m_lastSrcLat{nullptr}
    , m_newData{false}
    , m_bdLat{nullptr}
    , m_fdLat{nullptr}
    , m_lastBD{nullptr}
    , m_lastFD{nullptr}
    , m_newDeformation{false}
    , m_interpretation{Interpretation::SPATIAL}
    , m_lastInterpretation{Interpretation::NUM_INTERPRETATIONS}
    , m_newInterpretation{false}
    , m_connectivity{Connectivity::CORNER}
    , m_last3Dconnectivity{Connectivity::CORNER}
    , m_last2Dconnectivity{Connectivity::CORNER}
    , m_lastConnectivity{Connectivity::NUM_CONNECTIVITIES}
    , m_newConnectivity{false}
    , m_mergeMode{MergeMode::JOIN_TREE}
    , m_lastMergeMode{MergeMode::NUM_MERGEMODI}
    , m_newMergeMode{false}
    , m_sortNeighbours{SortNeighbours::BY_VALUE}
    , m_lastSortNeighbours{SortNeighbours::NUM_NEIGHBOURMODI}
    , m_newSortNeighbours{false}
    , m_lastGlobalMin{0.0f}
    , m_lastGlobalMax{0.0f}
    , m_newGlobalRange{false}
    , m_lastMin{0.0f}
    , m_lastMax{0.0f}
    , m_newRange{false}
    , m_persistenceMode{PersistenceMode::GLOBAL}
    , m_lastPersistenceMode{PersistenceMode::NUM_PERSISTANCEMODI}
    , m_newPersistenceMode{false}
    , m_lastGlobalPersistence{0.0f}
    , m_lastAdaptivePersistence{1.0f}
    , m_lastPersistence{0.0f}
    , m_newPersistence{false}
    , m_segmentationMode{SegmentationMode::DISJOINT}
    , m_lastSegmentationMode{SegmentationMode::NUM_SEGMENTATIONMODI}
    , m_newSegmentationMode{false}
    , m_lastNestedCoresWithSideBranches{false}
    , m_newNestedCoresWithSideBranches{false}
    , m_lattice3Mesh{nullptr}
    , m_mergeTree{nullptr}
{
    portBackwardDeformation.hide();
    portForwardDeformation.hide();

    portInterpretation.setLabel(static_cast<int>(Interpretation::SPATIAL), tr("Spatial"));
    portInterpretation.setLabel(static_cast<int>(Interpretation::XY_SLICES), tr("XY slices"));
    portInterpretation.setLabel(static_cast<int>(Interpretation::SPATIOTEMPORAL), tr("Spatiotemporal"));

    portConnectivity.setLabel(static_cast<int>(Connectivity::CORNER), tr("Corner (8+18)"));
    portConnectivity.setLabel(static_cast<int>(Connectivity::EDGE), tr("Edge (8+10)"));
    portConnectivity.setLabel(static_cast<int>(Connectivity::FACE), tr("Face (4+2)"));
    portConnectivity.setLabel(static_cast<int>(Connectivity::EXP_8P2), tr("8+2"));

    portMergeTree.setLabel(static_cast<int>(MergeMode::JOIN_TREE), tr("Join Tree"));
    portMergeTree.setLabel(static_cast<int>(MergeMode::SPLIT_TREE), tr("Split Tree"));

    portSortNeighbours.setLabel(static_cast<int>(SortNeighbours::BY_VALUE), tr("by Value"));
    portSortNeighbours.setLabel(static_cast<int>(SortNeighbours::BY_VOTE), tr("by Majority Vote"));
    portSortNeighbours.setLabel(static_cast<int>(SortNeighbours::BY_EXTREMA), tr("by Extrema"));

    portGlobalRange.setLabel(0, tr("Global Min."));
    portGlobalRange.setLabel(1, tr("Global Max."));
    portGlobalRange.setMinMax(0.0f, 0.0f);
    portGlobalRange.setValue(0, 0.0f);
    portGlobalRange.setValue(1, 0.0f);
    portGlobalRange.hide();

    portRange.setLabel(0, tr("Min."));
    portRange.setLabel(1, tr("Max."));
    portRange.setMinMax(0.0f, 0.0f);
    portRange.setValue(0, 0.0f);
    portRange.setValue(1, 0.0f);

    portPersistenceMode.setLabel(static_cast<int>(PersistenceMode::GLOBAL), tr("Global"));
    portPersistenceMode.setLabel(static_cast<int>(PersistenceMode::ADAPTIVE), tr("Adaptive"));

    portPersistence.setLabel(tr("Persistence"));
    portPersistence.setMinMax(0.0f, 0.0f);
    portPersistence.setValue(0.0f);

    portSegmentationMode.setLabel(static_cast<int>(SegmentationMode::DISJOINT), tr("Disjoint"));
    portSegmentationMode.setLabel(static_cast<int>(SegmentationMode::NESTED), tr("Nested"));
    portSegmentationMode.setLabel(static_cast<int>(SegmentationMode::NESTED_CORES), tr("Nested Cores"));

    portConsistentExtremaLabels.hide();
    portNestedCoresWithSideBranches.hide();
}


HxMergeTreeSegmentation::~HxMergeTreeSegmentation()
{
    if(m_lattice3Mesh)
        delete m_lattice3Mesh;
    if(m_mergeTree)
        delete m_mergeTree;
}


void HxMergeTreeSegmentation::update()
{
    if(portData.isNew() && portData.isNew() != 1)
    {
        bool initialNewData{!m_srcLat};
        m_srcLat = hxconnection_cast<HxLattice3>(portData);

        if(m_srcLat)
        {
            extractSrcLatInformation(m_srcLat);

            if(checkData())
            {
                if(m_srcLat->getDims().nz > 1)
                    portInterpretation.show();
                else
                {
                    portInterpretation.hide();
                    portInterpretation.setValue(static_cast<int>(Interpretation::XY_SLICES));
                }

                m_srcLat->computeRange(m_dataMin, m_dataMax);
                portGlobalRange.setMinMax(m_dataMin, m_dataMax);

                {
                    float min;
                    float max;

                    if(portUseGlobalRange.getValue())
                    {
                        portGlobalRange.show();
                        min = portGlobalRange.getValue(0);
                        max = portGlobalRange.getValue(1);
                    }
                    else
                    {
                        portGlobalRange.hide();
                        min = m_dataMin;
                        max = m_dataMax;
                    }

                    if(m_mergeMode == MergeMode::JOIN_TREE)
                    {
                        portRange.setMinMax(0, min, max);
                        portRange.setMinMax(1, max, max);
                    }
                    else
                    {
                        portRange.setMinMax(0, min, min);
                        portRange.setMinMax(1, min, max);
                    }
                }



                if(initialNewData)
                {
                    assert(m_lastGlobalMin == 0.0f && m_lastGlobalMax == 0.0f);
                    assert(m_lastMin == 0.0f && m_lastMax == 0.0f);

                    portGlobalRange.setValue(0, m_dataMin);
                    portGlobalRange.setValue(1, m_dataMax);

                    portRange.setValue(0, m_dataMin);
                    portRange.setValue(1, m_dataMax);


                    assert(m_persistenceMode == PersistenceMode::GLOBAL);
                    portPersistence.setMinMax(0, m_dataMax - m_dataMin);

                    assert(m_lastGlobalPersistence == 0.0f);
                    m_lastGlobalPersistence = m_dataMax - m_dataMin;
                    portPersistence.setValue(m_lastGlobalPersistence);
                }
                else if(m_persistenceMode == PersistenceMode::GLOBAL)
                {
                    portPersistence.setMinMax(0, m_dataMax - m_dataMin);
                }

                m_newData = true;
            }
            else // the object behind portData cannot be used
            {
                m_srcLat = nullptr;
                m_dataMin = 0.0f;
                m_dataMax = 0.0f;

                portGlobalRange.setMinMax(0.0f, 0.0f);
                portRange.setMinMax(0.0f, 0.0f);
                portPersistence.setMinMax(0.0f, 0.0f);

                portGlobalRange.setValue(0, 0.0f);
                portGlobalRange.setValue(1, 0.0f);

                portRange.setValue(0, 0.0f);
                portRange.setValue(1, 0.0f);

                portPersistence.setValue(0.0f);
            }
        }
        else // m_srcLat is nullptr
        {
            m_dataMin = 0.0f;
            m_dataMax = 0.0f;

            portGlobalRange.setMinMax(0.0f, 0.0f);
            portRange.setMinMax(0.0f, 0.0f);
            portPersistence.setMinMax(0.0f, 0.0f);

            portGlobalRange.setValue(0, 0.0f);
            portGlobalRange.setValue(1, 0.0f);

            portRange.setValue(0, 0.0f);
            portRange.setValue(1, 0.0f);

            portPersistence.setValue(0.0f);
        }
    }

    if(portBackwardDeformation.isNew() && portBackwardDeformation.isNew() != 1)
    {
        m_bdLat = hxconnection_cast<HxLattice3>(portBackwardDeformation);
        m_newDeformation = true;
    }

    if(portForwardDeformation.isNew() && portForwardDeformation.isNew() != 1)
    {
        m_fdLat = hxconnection_cast<HxLattice3>(portForwardDeformation);
        m_newDeformation = true;
    }

    if(portInterpretation.isNew() && portInterpretation.isNew() != 1)
    {
        m_interpretation = static_cast<Interpretation>(portInterpretation.getValue());

        if(m_interpretation == Interpretation::SPATIAL)
        {
            portBackwardDeformation.hide();
            portForwardDeformation.hide();
        }
        else
        {
            portBackwardDeformation.show();
            portForwardDeformation.show();
        }

        if(m_interpretation == Interpretation::XY_SLICES)
        {
            portConnectivity.setLabel(static_cast<int>(Connectivity::CORNER), tr("Corner (8)"));
            portConnectivity.setLabel(static_cast<int>(Connectivity::EDGE), tr("Edge (4)"));
            portConnectivity.setVisible(2, false);
            portConnectivity.setVisible(3, false);
            if( !(portConnectivity.isNew() && portConnectivity.isNew() != 1) )
                portConnectivity.setValue(static_cast<int>(m_last2Dconnectivity));
        }
        else
        {
            portConnectivity.setLabel(static_cast<int>(Connectivity::CORNER), tr("Corner (8+18)"));
            portConnectivity.setLabel(static_cast<int>(Connectivity::EDGE), tr("Edge (8+10)"));
            portConnectivity.setVisible(2, true);
            portConnectivity.setVisible(3, true);
            if( !(portConnectivity.isNew() && portConnectivity.isNew() != 1) )
                portConnectivity.setValue(static_cast<int>(m_last3Dconnectivity));
        }

        if(m_interpretation != m_lastInterpretation)
            m_newInterpretation = true;
        else
            m_newInterpretation = false;
    }

    if(portConnectivity.isNew() && portConnectivity.isNew() != 1)
    {
        m_connectivity = static_cast<Connectivity>(portConnectivity.getValue());

        if(m_interpretation == Interpretation::SPATIAL || m_interpretation == Interpretation::SPATIOTEMPORAL)
            m_last3Dconnectivity = m_connectivity;
        else if(m_interpretation == Interpretation::XY_SLICES)
            m_last2Dconnectivity = m_connectivity;

        if(m_connectivity != m_lastConnectivity)
            m_newConnectivity = true;
        else
            m_newConnectivity = false;
    }

    if(portMergeTree.isNew() && portMergeTree.isNew() != 1)
    {
        m_mergeMode = static_cast<MergeMode>(portMergeTree.getValue());

        float min;
        float max;

        if(portUseGlobalRange.getValue())
        {
            portGlobalRange.show();
            min = portGlobalRange.getValue(0);
            max = portGlobalRange.getValue(1);
        }
        else
        {
            portGlobalRange.hide();
            min = m_dataMin;
            max = m_dataMax;
        }

        if(m_mergeMode == MergeMode::JOIN_TREE)
        {
            portRange.setMinMax(0, min, max);
            portRange.setMinMax(1, max, max);
        }
        else
        {
            portRange.setMinMax(0, min, min);
            portRange.setMinMax(1, min, max);
        }

        if(m_mergeMode != m_lastMergeMode)
            m_newMergeMode = true;
        else
            m_newMergeMode = false;
    }

    if(portSortNeighbours.isNew() && portSortNeighbours.isNew() != 1)
    {
        m_sortNeighbours = static_cast<SortNeighbours>(portSortNeighbours.getValue());

        if(m_sortNeighbours != m_lastSortNeighbours)
            m_newSortNeighbours = true;
        else
            m_newSortNeighbours = false;
    }

    if(portUseGlobalRange.isNew() && portUseGlobalRange.isNew() != 1)
    {
        float min;
        float max;

        if(portUseGlobalRange.getValue())
        {
            portGlobalRange.show();
            min = portGlobalRange.getValue(0);
            max = portGlobalRange.getValue(1);
        }
        else
        {
            portGlobalRange.hide();
            min = m_dataMin;
            max = m_dataMax;
        }

        if(m_mergeMode == MergeMode::JOIN_TREE)
        {
            portRange.setMinMax(0, min, max);
            portRange.setMinMax(1, max, max);
        }
        else
        {
            portRange.setMinMax(0, min, min);
            portRange.setMinMax(1, min, max);
        }

        if(min != m_lastGlobalMin || max != m_lastGlobalMax)
            m_newGlobalRange = true;
        else
            m_newGlobalRange = false;
    }

    if(portGlobalRange.isNew() && portGlobalRange.isNew() != 1 && portUseGlobalRange.getValue())
    {
        assert(portGlobalRange.isVisible());

        float globalMin = portGlobalRange.getValue(0);
        float globalMax = portGlobalRange.getValue(1);

        if(m_mergeMode == MergeMode::JOIN_TREE)
        {
            portRange.setMinMax(0, globalMin, globalMax);
            portRange.setMinMax(1, globalMax, globalMax);
        }
        else
        {
            portRange.setMinMax(0, globalMin, globalMin);
            portRange.setMinMax(1, globalMin, globalMax);
        }

        if(globalMin != m_lastGlobalMin || globalMax != m_lastGlobalMax)
            m_newGlobalRange = true;
        else
            m_newGlobalRange = false;
    }

    if(portRange.isNew() && portRange.isNew() != 1)
    {
        float min = portRange.getValue(0);
        float max = portRange.getValue(1);

        if(min != m_lastMin || max != m_lastMax)
            m_newRange = true;
        else
            m_newRange = false;
    }

    if(portPersistenceMode.isNew() && portPersistenceMode.isNew() != 1)
    {
        m_persistenceMode = static_cast<PersistenceMode>(portPersistenceMode.getValue());

        if(m_persistenceMode == PersistenceMode::GLOBAL)
        {
            portPersistence.setMinMax(0, m_dataMax - m_dataMin);

            if( !(portPersistence.isNew() && portPersistence.isNew() != 1) )
                portPersistence.setValue(m_lastGlobalPersistence);
        }
        else // m_persistenceMode == PersistenceMode::ADAPTIVE)
        {
            portPersistence.setMinMax(0, 1);

            if( !(portPersistence.isNew() && portPersistence.isNew() != 1) )
                portPersistence.setValue(m_lastAdaptivePersistence);
        }

        if(m_persistenceMode != m_lastPersistenceMode)
            m_newPersistenceMode = true;
        else
            m_newPersistenceMode = false;
    }

    if(portPersistence.isNew() && portPersistence.isNew() != 1)
    {
        float persistence = portPersistence.getValue();

        if(m_persistenceMode == PersistenceMode::GLOBAL)
            m_lastGlobalPersistence = persistence;
        else if(m_persistenceMode == PersistenceMode::ADAPTIVE)
            m_lastAdaptivePersistence = persistence;

        if(persistence != m_lastPersistence)
            m_newPersistence = true;
        else
            m_newPersistence = false;
    }

    if(portSegmentationMode.isNew() && portSegmentationMode.isNew() != 1)
    {
        m_segmentationMode = static_cast<SegmentationMode>(portSegmentationMode.getValue());

        if(m_segmentationMode == SegmentationMode::NESTED || m_segmentationMode == SegmentationMode::NESTED_CORES)
        {
            portConsistentExtremaLabels.show();

            if(m_segmentationMode == SegmentationMode::NESTED_CORES)
                portNestedCoresWithSideBranches.show();
            else
                portNestedCoresWithSideBranches.hide();
        }
        else
        {
            portConsistentExtremaLabels.hide();
            portNestedCoresWithSideBranches.hide();
        }


        if(m_segmentationMode != m_lastSegmentationMode)
            m_newSegmentationMode = true;
        else
            m_newSegmentationMode = false;
    }

    if(portNestedCoresWithSideBranches.isNew() && portNestedCoresWithSideBranches.isNew() != 1)
    {
        bool nestedCoresWithSideBranches = portNestedCoresWithSideBranches.getValue();

        if(nestedCoresWithSideBranches != m_lastNestedCoresWithSideBranches)
            m_newNestedCoresWithSideBranches = true;
        else
            m_newNestedCoresWithSideBranches = false;
    }

    if(portConsistentExtremaLabels.isNew() && portConsistentExtremaLabels.isNew() != 1)
    {
        bool consistentExtremaLabels = portConsistentExtremaLabels.getValue();

        if(consistentExtremaLabels != m_lastConsistentExtremaLabels)
            m_newConsistentExtremaLabels = true;
        else
            m_newConsistentExtremaLabels = false;
    }
}


void HxMergeTreeSegmentation::compute()
{
    if(!portDoIt.wasHit())
        return;

    if(!m_srcLat)
        return;

    if(!m_newData && !m_newDeformation && !m_newInterpretation && !m_newConnectivity && !m_newMergeMode && !m_newSortNeighbours && !m_newGlobalRange &&
       !m_newRange && !m_newPersistenceMode && !m_newPersistence && !m_newSegmentationMode && !m_newNestedCoresWithSideBranches && !m_newConsistentExtremaLabels)
        return;


    if(m_newData)
    {
        assert( (!m_lattice3Mesh && !m_mergeTree) || (m_lattice3Mesh && m_mergeTree) );

        if(m_lattice3Mesh && m_mergeTree)
        {
            delete m_lattice3Mesh;
            delete m_mergeTree;
        }

        m_lastSrcLat = m_srcLat;

        switch(m_srcLatPrimType)
        {
            case McPrimType::MC_INT8:
            {
                Lattice3Mesh<char>* lattice3Mesh = new Lattice3Mesh<char>(m_lastSrcLat);

                m_lattice3Mesh = dynamic_cast<Lattice3MeshBase*>(lattice3Mesh);
                m_mergeTree = new MergeTree<char>(lattice3Mesh);
                break;
            }
            case McPrimType::MC_UINT8:
            {
                Lattice3Mesh<unsigned char>* lattice3Mesh = new Lattice3Mesh<unsigned char>(m_lastSrcLat);

                m_lattice3Mesh = dynamic_cast<Lattice3MeshBase*>(lattice3Mesh);
                m_mergeTree = new MergeTree<unsigned char>(lattice3Mesh);
                break;
            }
            case McPrimType::MC_INT16:
            {
                Lattice3Mesh<short>* lattice3Mesh = new Lattice3Mesh<short>(m_lastSrcLat);

                m_lattice3Mesh = dynamic_cast<Lattice3MeshBase*>(lattice3Mesh);
                m_mergeTree = new MergeTree<short>(lattice3Mesh);
                break;
            }
            case McPrimType::MC_UINT16:
            {
                Lattice3Mesh<unsigned short>* lattice3Mesh = new Lattice3Mesh<unsigned short>(m_lastSrcLat);

                m_lattice3Mesh = dynamic_cast<Lattice3MeshBase*>(lattice3Mesh);
                m_mergeTree = new MergeTree<unsigned short>(lattice3Mesh);
                break;
            }
            case McPrimType::MC_INT32:
            {
                Lattice3Mesh<int>* lattice3Mesh = new Lattice3Mesh<int>(m_lastSrcLat);

                m_lattice3Mesh = dynamic_cast<Lattice3MeshBase*>(lattice3Mesh);
                m_mergeTree = new MergeTree<int>(lattice3Mesh);
                break;
            }
            case McPrimType::MC_UINT32:
            {
                Lattice3Mesh<unsigned int>* lattice3Mesh = new Lattice3Mesh<unsigned int>(m_lastSrcLat);

                m_lattice3Mesh = dynamic_cast<Lattice3MeshBase*>(lattice3Mesh);
                m_mergeTree = new MergeTree<unsigned int>(lattice3Mesh);
                break;
            }
            case McPrimType::MC_INT64:
            {
                Lattice3Mesh<long long>* lattice3Mesh = new Lattice3Mesh<long long>(m_lastSrcLat);

                m_lattice3Mesh = dynamic_cast<Lattice3MeshBase*>(lattice3Mesh);
                m_mergeTree = new MergeTree<long long>(lattice3Mesh);
                break;
            }
            case McPrimType::MC_UINT64:
            {
                Lattice3Mesh<unsigned long long>* lattice3Mesh = new Lattice3Mesh<unsigned long long>(m_lastSrcLat);

                m_lattice3Mesh = dynamic_cast<Lattice3MeshBase*>(lattice3Mesh);
                m_mergeTree = new MergeTree<unsigned long long>(lattice3Mesh);
                break;
            }

            case McPrimType::MC_FLOAT:
            {
                Lattice3Mesh<float>* lattice3Mesh = new Lattice3Mesh<float>(m_lastSrcLat);

                m_lattice3Mesh = dynamic_cast<Lattice3MeshBase*>(lattice3Mesh);
                m_mergeTree = new MergeTree<float>(lattice3Mesh);
                break;
            }
            case McPrimType::MC_DOUBLE:
            {
                Lattice3Mesh<double>* lattice3Mesh = new Lattice3Mesh<double>(m_lastSrcLat);

                m_lattice3Mesh = dynamic_cast<Lattice3MeshBase*>(lattice3Mesh);
                m_mergeTree = new MergeTree<double>(lattice3Mesh);
                break;
            }
            default:
                return;
        }

        if(m_bdLat && m_fdLat && checkDeformation())
        {
            m_lastBD = m_bdLat;
            m_lastFD = m_fdLat;
            m_lattice3Mesh->setDeformation(m_lastBD, m_lastFD);
        }
        else
        {
            m_lastBD = nullptr;
            m_lastFD = nullptr;
            // no need to call m_lattice3Mesh->unsetDeformation() because a deformation is not set by default
        }

        m_lastInterpretation = m_interpretation;
        m_lattice3Mesh->setInterpretation(m_lastInterpretation);

        m_lastConnectivity = m_connectivity;
        m_lattice3Mesh->setConnectivity(m_lastConnectivity);

        m_lastMergeMode = m_mergeMode;
        m_mergeTree->setMergeMode(m_lastMergeMode);

        m_lastSortNeighbours = m_sortNeighbours;
        m_mergeTree->setSortNeighbours(m_lastSortNeighbours);

        if(portUseGlobalRange.getValue())
        {
            assert(portGlobalRange.isVisible());

            m_lastGlobalMin = portGlobalRange.getValue(0);
            m_lastGlobalMax = portGlobalRange.getValue(1);
            m_lattice3Mesh->setGlobalRange(m_lastGlobalMin, m_lastGlobalMax);
        }
        else
        {
            assert(!portGlobalRange.isVisible());

            m_lastGlobalMin = m_dataMin;
            m_lastGlobalMax = m_dataMax;
            // no need to call m_lattice3Mesh->unsetGlobalRange() because a global range is not set by default
        }

        m_newData = false;
        m_newDeformation = false;
        m_newInterpretation = false;
        m_newConnectivity = false;
        m_newMergeMode = false;
        m_newSortNeighbours = false;
        m_newGlobalRange = false;

        m_lattice3Mesh->sortNodeIndices();
        m_mergeTree->computeMergeTree(m_lastGlobalMin, m_lastGlobalMax);


        m_lastMin = portRange.getValue(0);
        m_lastMax = portRange.getValue(1);

        m_lastPersistenceMode = m_persistenceMode;
        m_mergeTree->setPersistenceMode(m_lastPersistenceMode);

        m_lastPersistence = portPersistence.getValue();

        m_lastSegmentationMode = m_segmentationMode;
        m_mergeTree->setSegmentationMode(m_lastSegmentationMode);

        m_lastConsistentExtremaLabels = portConsistentExtremaLabels.getValue();
        m_mergeTree->setConsistentExtremaLabels(m_lastConsistentExtremaLabels);

        m_lastNestedCoresWithSideBranches = portNestedCoresWithSideBranches.getValue();
        m_mergeTree->setNestedCoresWithSideBranches(m_lastNestedCoresWithSideBranches);


        m_newRange = false;
        m_newPersistenceMode = false;
        m_newPersistence = false;
        m_newSegmentationMode = false;
        m_newNestedCoresWithSideBranches = false;
    }
    else if(m_newDeformation || m_newInterpretation || m_newConnectivity || m_newMergeMode || m_newSortNeighbours || m_newGlobalRange)
    {
        assert(m_lattice3Mesh && m_mergeTree);

        if(m_newDeformation)
        {
            if(m_bdLat && m_fdLat && checkDeformation())
            {
                m_lastBD = m_bdLat;
                m_lastFD = m_fdLat;
                m_lattice3Mesh->setDeformation(m_lastBD, m_lastFD);
            }
            else
            {
                m_lastBD = nullptr;
                m_lastFD = nullptr;
                m_lattice3Mesh->unsetDeformation();
            }
            m_newDeformation = false;
        }

        if(m_newInterpretation)
        {
            m_lastInterpretation = m_interpretation;
            m_lattice3Mesh->setInterpretation(m_lastInterpretation);
            m_newInterpretation = false;
        }

        if(m_newConnectivity)
        {
            m_lastConnectivity = m_connectivity;
            m_lattice3Mesh->setConnectivity(m_lastConnectivity);
            m_newConnectivity = false;
        }

        if(m_newMergeMode)
        {
            m_lastMergeMode = m_mergeMode;
            m_mergeTree->setMergeMode(m_lastMergeMode);
            m_newMergeMode = false;
        }

        if(m_newSortNeighbours)
        {
            m_lastSortNeighbours = m_sortNeighbours;
            m_mergeTree->setSortNeighbours(m_lastSortNeighbours);
            m_newSortNeighbours = false;
        }

        if(m_newGlobalRange)
        {
            if(portUseGlobalRange.getValue())
            {
                m_lastGlobalMin = portGlobalRange.getValue(0);
                m_lastGlobalMax = portGlobalRange.getValue(1);
                m_lattice3Mesh->setGlobalRange(m_lastGlobalMin, m_lastGlobalMax);
            }
            else
            {
                m_lastGlobalMin = m_dataMin;
                m_lastGlobalMax = m_dataMax;
                m_lattice3Mesh->unsetGlobalRange();
            }
            m_lattice3Mesh->sortNodeIndices();
            m_newGlobalRange = false;
        }

        m_mergeTree->computeMergeTree(m_lastGlobalMin, m_lastGlobalMax);


        if(m_newRange)
        {
            m_lastMin = portRange.getValue(0);
            m_lastMax = portRange.getValue(1);
            m_newRange = false;
        }

        if(m_newPersistenceMode)
        {
            m_lastPersistenceMode = m_persistenceMode;
            m_mergeTree->setPersistenceMode(m_lastPersistenceMode);
            m_newPersistenceMode = false;
        }

        if(m_newPersistence)
        {
            m_lastPersistence = portPersistence.getValue();
            m_newPersistence = false;
        }

        if(m_newSegmentationMode)
        {
            m_lastSegmentationMode = m_segmentationMode;
            m_mergeTree->setSegmentationMode(m_lastSegmentationMode);
            m_newSegmentationMode = false;
        }

        if(m_newConsistentExtremaLabels)
        {
            m_lastConsistentExtremaLabels = portConsistentExtremaLabels.getValue();
            m_mergeTree->setConsistentExtremaLabels(m_lastConsistentExtremaLabels);
            m_newConsistentExtremaLabels = false;
        }

        if(m_newNestedCoresWithSideBranches)
        {
            m_lastNestedCoresWithSideBranches = portNestedCoresWithSideBranches.getValue();
            m_mergeTree->setNestedCoresWithSideBranches(m_lastNestedCoresWithSideBranches);
            m_newNestedCoresWithSideBranches = false;
        }
    }
    else // m_newRange || m_newPersistenceMode || m_newPersistence || m_newNestedHierarchy || m_newNestedCoresWithSideBranches
    {
        if(m_newRange)
        {
            m_lastMin = portRange.getValue(0);
            m_lastMax = portRange.getValue(1);
            m_newRange = false;
        }

        if(m_newPersistenceMode)
        {
            m_lastPersistenceMode = m_persistenceMode;
            m_mergeTree->setPersistenceMode(m_lastPersistenceMode);
            m_newPersistenceMode = false;
        }

        if(m_newPersistence)
        {
            m_lastPersistence = portPersistence.getValue();
            m_newPersistence = false;
        }

        if(m_newSegmentationMode)
        {
            m_lastSegmentationMode = m_segmentationMode;
            m_mergeTree->setSegmentationMode(m_lastSegmentationMode);
            m_newSegmentationMode = false;
        }

        if(m_newConsistentExtremaLabels)
        {
            m_lastConsistentExtremaLabels = portConsistentExtremaLabels.getValue();
            m_mergeTree->setConsistentExtremaLabels(m_lastConsistentExtremaLabels);
            m_newConsistentExtremaLabels = false;
        }

        if(m_newNestedCoresWithSideBranches)
        {
            m_lastNestedCoresWithSideBranches = portNestedCoresWithSideBranches.getValue();
            m_mergeTree->setNestedCoresWithSideBranches(m_lastNestedCoresWithSideBranches);
            m_newNestedCoresWithSideBranches = false;
        }
    }


    HxUniformLabelField3* res{createOrReuseResult()};

    m_mergeTree->computeFastMergeSegmentation(m_lastMin, m_lastMax, m_lastPersistence, res);

    res->touchMinMax();
    res->composeLabel(portData.getSource()->getLabel(), tr("mergeLabeling"));
    setResult(res);
}


void HxMergeTreeSegmentation::extractSrcLatInformation(const HxLattice3* srcLat)
{
    m_srcLatDims = srcLat->getDims();
    m_srcLatNDataVar = srcLat->nDataVar();
    m_srcLatPrimType = srcLat->primType();
    m_srcLatCoordType = srcLat->coords()->coordType();
    m_srcLatBBox = srcLat->coords()->getBoundingBox();
}


bool HxMergeTreeSegmentation::checkData()
{
    if(m_srcLatNDataVar != 1)
    {
        std::cout << "For this module, the incoming dataset must be scalar data." << std::endl;
        return false;
    }
    if(m_srcLatCoordType != C_UNIFORM)
    {
        std::cout << "For this module, the incoming dataset must have uniform coordinates." << std::endl;
        return false;
    }
    if(m_srcLatDims.nx < 1 || m_srcLatDims.ny < 1 || m_srcLatDims.nz < 1)
    {
        std::cout << "For this module, the incoming dataset must have a non-degenerated lattice." << std::endl;
        return false;
    }

    return true;
}


bool HxMergeTreeSegmentation::checkDeformation()
{
    assert(m_bdLat);
    assert(m_fdLat);

    if(m_bdLat->primType() != McPrimType(3) || m_fdLat->primType() != McPrimType(3))
    {
        std::cout << "primtyp of deformations must be float" << std::endl;
        return false;
    }
    if(m_bdLat->getDims() != m_srcLatDims || m_fdLat->getDims() != m_srcLatDims)
    {
        std::cout << "dimension do not fit" << std::endl;
        return false;
    }
    if(m_bdLat->coords()->coordType() != m_srcLatCoordType || m_fdLat->coords()->coordType() != m_srcLatCoordType)
    {
        std::cout << "coord type of deformations does not fit data coord type" << std::endl;
        return false;
    }
    if(m_bdLat->nDataVar() < 2 || m_fdLat->nDataVar() < 2)
    {
        std::cout << "deformations fields must at least have two components" << std::endl;
        return false;
    }

    return true;
}


HxUniformLabelField3* HxMergeTreeSegmentation::createOrReuseResult()
{
    HxUniformLabelField3* res;
    HxLabelLattice3* resLat = NULL;

    res = dynamic_cast<HxUniformLabelField3*>(getResult());

    if(res)
    {
        resLat = mcinterface_cast<HxLabelLattice3>(res);

        if(resLat)
        {
            if(resLat->nDataVar() != 1 || resLat->coords()->coordType() != C_UNIFORM)
            {
                res = NULL;
                resLat = NULL;
            }
        }
        else
        {
            res = NULL;
        }
    }

    if(res)
    {
        assert(resLat);
        resLat->init(m_srcLatDims, 1, McPrimType::MC_INT32, C_UNIFORM);
    }
    else
    {
        assert(!resLat);
        resLat = new HxLabelLattice3(m_srcLatDims, McPrimType::MC_INT32, C_UNIFORM);
        res = new HxUniformLabelField3(resLat);
    }

    resLat->setBoundingBox(m_srcLatBBox);

    return res;
}
