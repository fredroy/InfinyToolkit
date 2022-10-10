/*****************************************************************************
 *            Copyright (C) - InfinyTech3D - All Rights Reserved             *
 *                                                                           *
 * Unauthorized copying of this file, via any medium is strictly prohibited  *
 * Proprietary and confidential.                                             *
 *                                                                           *
 * Written by Erik Pernod <erik.pernod@infinytech3d.com>, October 2019       *
 ****************************************************************************/

#include <InteractionTools/CarvingTools/AdvancedCarvingManager.h>
#include <sofa/core/ObjectFactory.h>
#include <sofa/core/collision/DetectionOutput.h>
#include <sofa/core/objectmodel/KeypressedEvent.h>
#include <sofa/core/objectmodel/KeyreleasedEvent.h>
#include <sofa/core/objectmodel/ScriptEvent.h>
#include <sofa/core/objectmodel/MouseEvent.h>
#include <sofa/simulation/AnimateBeginEvent.h>
#include <sofa/simulation/AnimateEndEvent.h>
#include <sofa/simulation/CollisionEndEvent.h>

#include <sofa/core/topology/TopologicalMapping.h>
#include <sofa/helper/AdvancedTimer.h>

#include <InteractionTools/CarvingTools/SurfaceCarvingPerformer.h>
#include <InteractionTools/CarvingTools/BurningPerformer.h>

#ifdef HAS_MESHREFINEMENT_PLUGIN
#include <InteractionTools/CarvingTools/RefineCarvingPerformer.h>
#endif

#include <sofa/core/behavior/BaseMechanicalState.h>

#include <sofa/core/topology/TopologyData.inl>


namespace sofa::component::collision
{

using namespace sofa::core::topology;

int AdvancedCarvingManagerClass = core::RegisterObject("Manager handling carving operations between a tool and an object.")
.add< AdvancedCarvingManager >()
;


AdvancedCarvingManager::AdvancedCarvingManager()
    : d_toolModelPath( initData(&d_toolModelPath, "toolModelPath", "Tool model path"))
    , d_surfaceModelPath( initData(&d_surfaceModelPath, "surfaceModelPath", "TriangleSetModel or SphereModel path"))
    , d_active( initData(&d_active, false, "active", "Activate this object.\nNote that this can be dynamically controlled by using a key") )
    , d_carvingWithBurning(initData(&d_carvingWithBurning, true, "carvingWithBurning", "Activate this object.\nNote that this can be dynamically controlled by using a key"))
    , d_carvingWithRefinement(initData(&d_carvingWithRefinement, false, "carvingWithRefinement", "Activate this object.\nNote that this can be dynamically controlled by using a key"))
    , d_carvingDistance( initData(&d_carvingDistance, 0.0, "carvingDistance", "Collision distance at which cavring will start. Equal to contactDistance by default."))    
    , d_refineDistance(initData(&d_refineDistance, 0.0, "refineDistance", "Collision distance at which cavring will start. Equal to contactDistance by default."))    
    , d_refineCriteria( initData(&d_refineCriteria, 0.5, "refineCriteria", "Collision distance at which cavring will start. Equal to contactDistance by default."))
    , d_carvingSpeed(initData(&d_carvingSpeed, 0.001, "carvingSpeed", "Collision distance at which cavring will start. Equal to contactDistance by default."))
    , d_refineThreshold(initData(&d_refineThreshold, 1.0, "refineThreshold", "Collision distance at which cavring will start. Equal to contactDistance by default."))
    , m_testID(initData(&m_testID, sofa::type::vector<unsigned int>(), "testID", "Scale of the terahedra (between 0 and 1; if <1.0, it produces gaps between the tetrahedra)"))
    , m_vtexcoords(initData(&m_vtexcoords, "texcoords", "coordinates of the texture"))
    , d_drawContacts( initData(&d_drawContacts, false, "drawContacts", "Activate this object.\nNote that this can be dynamically controlled by using a key") )
{
    this->f_listening.setValue(true);
}


AdvancedCarvingManager::~AdvancedCarvingManager()
{
    for (unsigned int i = 0; i < m_carvingPerformer.size(); i++)
    {
        delete m_carvingPerformer[i];
        m_carvingPerformer[i] = nullptr;
    }
    m_carvingPerformer.clear();
}


void AdvancedCarvingManager::bwdInit()
{
    // Search for collision model corresponding to the tool.
    if (d_toolModelPath.getValue().empty())
        m_toolCollisionModel = getContext()->get<ToolCollisionModel>(core::objectmodel::Tag("CarvingTool"), core::objectmodel::BaseContext::SearchRoot);
    else
        m_toolCollisionModel = getContext()->get<ToolCollisionModel>(d_toolModelPath.getValue());

    // Search for the surface collision model.
    if (d_surfaceModelPath.getValue().empty())
    {
        // we look for a CollisionModel relying on a TetrahedronSetTopology.
        std::vector<SurfaceCollisionModel*> models;
        getContext()->get<SurfaceCollisionModel>(&models, core::objectmodel::Tag("CarvingSurface"), core::objectmodel::BaseContext::SearchRoot);
        
        // If topological mapping, iterate into child Node to find mapped topology
        for (size_t i=0; i<models.size(); ++i)
        {
            core::CollisionModel* m = models[i];
            m_surfaceCollisionModels.push_back(m);
        }
    }
    else
    {
        m_surfaceCollisionModels.push_back(getContext()->get<core::CollisionModel>(d_surfaceModelPath.getValue()));
    }


    m_detectionNP = getContext()->get<core::collision::NarrowPhaseDetection>();
    m_carvingReady = true;

    if (m_toolCollisionModel == nullptr) { msg_error() << "m_toolCollisionModel not found"; m_carvingReady = false; }
    if (m_surfaceCollisionModels.empty()) { msg_error() << "m_surfaceCollisionModels not found"; m_carvingReady = false; }
    if (m_detectionNP == nullptr) { msg_error() << "NarrowPhaseDetection not found"; m_carvingReady = false; }
    
    if (!m_carvingReady) {
        msg_error() << "AdvancedCarvingManager: initialisation failed.";
        return;
    }

    for (auto surfCol : m_surfaceCollisionModels)
    {
        // check if mapping is present
        sofa::core::topology::TopologicalMapping* topoMapping;
        surfCol->getContext()->get(topoMapping);

        TetrahedronSetTopologyContainer::SPtr topo = nullptr;
        if (topoMapping != nullptr)
        {
            topo = dynamic_cast<TetrahedronSetTopologyContainer*> (topoMapping->getFrom());
        }
        else
            topo = dynamic_cast<TetrahedronSetTopologyContainer*> (surfCol->getCollisionTopology());

        if (topo == nullptr)
        {
            msg_error() << "m_surfaceCollisionModels: " << surfCol->getName() << " doesn't have a TetrahedronSetTopologyContainer. No carving will be performed";
            continue;
        }
        
        bool alreadyRegistered = false;
        for (auto carvingPerformer : m_carvingPerformer)
        {
            if (carvingPerformer->getTopology() == topo)
            {
                alreadyRegistered = true;
                break;
            }
        }

        if (!alreadyRegistered && d_carvingWithBurning.getValue()) {
            m_carvingPerformer.push_back(new BurningPerformer(topo, this));
        }

        if (!alreadyRegistered && d_carvingWithRefinement.getValue()) {
#ifdef HAS_MESHREFINEMENT_PLUGIN
            m_carvingPerformer.push_back(new RefineCarvingPerformer(topo, this));
#else
            msg_warning() << "Option carvingWithRefinement require MeshRefienement plugin. Please check https://www.sofa-framework.org/applications/marketplace/cutting-mesh-refinement/ for more information.";
#endif
        }
    }


    for (auto carvingPerformer : m_carvingPerformer)
    {
        carvingPerformer->initPerformer();
    }

    msg_warning() << "bwdInit!";
}


void AdvancedCarvingManager::clearContacts()
{
    for (auto carvingPerformer : m_carvingPerformer)
    {
        carvingPerformer->clearContacts();
    }
}


void AdvancedCarvingManager::filterCollision()
{
    if (!m_carvingReady)
        return;

    if (!d_active.getValue())
        return;

    lockConstraints.lock();
    clearContacts();

    // get the collision output
    const core::collision::NarrowPhaseDetection::DetectionOutputMap& detectionOutputs = m_detectionNP->getDetectionOutputs();
    if (detectionOutputs.size() == 0) // exit if no collision
    {
        lockConstraints.unlock();
        return;
    }

    const SReal& refDistance = d_refineDistance.getValue();
    SReal invRefDistance = 1 / refDistance;

    // loop on the contact to get the one between the CarvingSurface and the CarvingTool collision model
    const ContactVector* contacts = nullptr;
    for (core::collision::NarrowPhaseDetection::DetectionOutputMap::const_iterator it = detectionOutputs.begin(); it != detectionOutputs.end(); ++it)
    {
        sofa::core::CollisionModel* collMod1 = it->first.first;
        sofa::core::CollisionModel* collMod2 = it->first.second;
        sofa::core::CollisionModel* targetModel = nullptr;

        // get the good collision contact
        if (collMod1 == m_toolCollisionModel && collMod2->hasTag(sofa::core::objectmodel::Tag("CarvingSurface")))
        {
            targetModel = collMod2;
        }
        else if (collMod2 == m_toolCollisionModel && collMod1->hasTag(sofa::core::objectmodel::Tag("CarvingSurface")))
        {
            targetModel = collMod1;
        }
        else
            continue;


        // Get the number of contacts        
        contacts = dynamic_cast<const ContactVector*>(it->second);
        size_t ncontacts = contacts->size();
        if (contacts == nullptr || contacts->size() == 0)
            continue;


        int mode = -1; // 0 = triangleModel, 1 = pointModel
        if (targetModel->getTypeName().find("TriangleCollisionModel") != std::string::npos)
        {
            mode = 0;
        }
        else if (targetModel->getTypeName().find("PointCollisionModel") != std::string::npos)
        {
            mode = 1;
        }
        else
        {
            msg_error() << targetModel->getTypeName() << " collision model not handled";
            continue;
        }
        
        
        // check if mapping is present
        sofa::core::topology::TopologicalMapping* topoMapping;
        targetModel->getContext()->get(topoMapping);

        auto currentTopo = targetModel->getCollisionTopology();
        if (topoMapping != nullptr)
            currentTopo = topoMapping->getFrom();


        for (auto carvingPerformer : m_carvingPerformer)
        {
            if (carvingPerformer->getTopology() != currentTopo) // not the same topo, carving performer not on this collisionModel
                continue;

            BaseCarvingPerformer* performer = carvingPerformer;

            for (size_t j = 0; j < ncontacts; ++j)
            {
                const ContactVector::value_type& c = (*contacts)[j];
                int elemIdx = (c.elem.first.getCollisionModel() == m_toolCollisionModel ? c.elem.second.getIndex() : c.elem.first.getIndex());

                // update the triangle id if a mapping is present
                contactInfo* info = new contactInfo();

                if (mode == 0 && topoMapping != nullptr)
                {
                    elemIdx = topoMapping->getGlobIndex(elemIdx);
                }

                info->elemId = elemIdx;
                info->normal = c.normal;
                info->pointA = c.point[0];
                info->pointB = c.point[1];
                info->dist = c.value;


                if (mode == 0)
                    performer->m_triangleContacts.push_back(info);
                else if (mode == 1)
                    performer->m_pointContacts.push_back(info);
            }

        }      
    }

    lockConstraints.unlock();

    for (auto carvingPerformer : m_carvingPerformer)
    {
        carvingPerformer->filterContacts();
    }
    
    // process the collision
    for (auto carvingPerformer : m_carvingPerformer)
    {
        carvingPerformer->runPerformer();
    }
}



void AdvancedCarvingManager::handleEvent(sofa::core::objectmodel::Event* event)
{
    if (!m_carvingReady)
        return;

    if (simulation::AnimateEndEvent::checkEventType(event))
    {
        filterCollision();
    }
    else if (sofa::core::objectmodel::HapticDeviceEvent * ev = dynamic_cast<sofa::core::objectmodel::HapticDeviceEvent *>(event))
    {
        if (ev->getButtonState() == 1) d_active.setValue(true);
        else if (ev->getButtonState() == 0) d_active.setValue(false);
    }

    if (sofa::core::objectmodel::KeypressedEvent* ev = dynamic_cast<sofa::core::objectmodel::KeypressedEvent*>(event))
    {
        if (ev->getKey() == 'C')
        {
            msg_warning() << "Burn, baby burn!";
            d_active.setValue(true);
        }
    }
}

void AdvancedCarvingManager::draw(const core::visual::VisualParams* vparams)
{
    if (!m_carvingReady)
        return;

    if (!d_drawContacts.getValue())
        return;

    for (auto carvingPerformer : m_carvingPerformer)
    {
        carvingPerformer->draw(vparams);
    }
}


} // namespace sofa::component::collision
