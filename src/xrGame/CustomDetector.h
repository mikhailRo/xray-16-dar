#pragma once
#include "inventory_item_object.h"
#include "xrEngine/Feel_Touch.h"
#include "HudSound.h"
#include "CustomZone.h"
#include "Artefact.h"
#include "ai_sounds.h"

class CCustomZone;
class CInventoryOwner;

struct ITEM_TYPE
{
    Fvector2 freq; // min,max
    HUD_SOUND_ITEM detect_snds;

    shared_str zone_map_location;
    shared_str nightvision_particle;
};

//описание зоны, обнаруженной детектором
struct ITEM_INFO
{
    ITEM_TYPE* curr_ref;
    float snd_time;
    //текущая частота работы датчика
    float cur_period;
    // particle for night-vision mode
    CParticlesObject* pParticle;

    ITEM_INFO();
    ~ITEM_INFO();
};

template <typename K>
class CDetectList : public Feel::Touch
{
protected:
    typedef xr_map<shared_str, ITEM_TYPE> TypesMap;
    typedef typename TypesMap::iterator TypesMapIt;
    TypesMap m_TypesMap;

public:
    typedef xr_map<K*, ITEM_INFO> ItemsMap;
    typedef typename ItemsMap::iterator ItemsMapIt;
    ItemsMap m_ItemInfos;

protected:
    virtual void feel_touch_new(IGameObject* O)
    {
        K* pK = smart_cast<K*>(O);
        R_ASSERT(pK);
        TypesMapIt it = m_TypesMap.find(O->cNameSect());
        R_ASSERT(it != m_TypesMap.end());
        m_ItemInfos[pK].snd_time = 0.0f;
        m_ItemInfos[pK].curr_ref = &(it->second);
    }
    virtual void feel_touch_delete(IGameObject* O)
    {
        K* pK = smart_cast<K*>(O);
        R_ASSERT(pK);
        m_ItemInfos.erase(pK);
    }

public:
    void destroy()
    {
        TypesMapIt it = m_TypesMap.begin();
        for (; it != m_TypesMap.end(); ++it)
            HUD_SOUND_ITEM::DestroySound(it->second.detect_snds);
    }
    void clear()
    {
        m_ItemInfos.clear();
        Feel::Touch::feel_touch.clear();
    }
    virtual void load(LPCSTR sect, LPCSTR prefix)
    {
        u32 i = 1;
        string256 temp;
        do
        {
            xr_sprintf(temp, "%s_class_%d", prefix, i);
            if (pSettings->line_exist(sect, temp))
            {
                shared_str item_sect = pSettings->r_string(sect, temp);

                m_TypesMap.insert(std::make_pair(item_sect, ITEM_TYPE()));
                ITEM_TYPE& item_type = m_TypesMap[item_sect];

                xr_sprintf(temp, "%s_freq_%d", prefix, i);
                item_type.freq = pSettings->r_fvector2(sect, temp);

                xr_sprintf(temp, "%s_sound_%d_", prefix, i);
                HUD_SOUND_ITEM::LoadSound(sect, temp, item_type.detect_snds, SOUND_TYPE_ITEM);

                ++i;
            }
            else
                break;

        } while (true);
    }
};

class CAfList : public CDetectList<CArtefact>
{
protected:
    virtual bool feel_touch_contact(IGameObject* O);

public:
    CAfList() : m_af_rank(0) {}
    int m_af_rank;
};

class CUIArtefactDetectorBase;
class CLAItem;

class CCustomDetector : public CHudItemObject
{
    typedef CHudItemObject inherited;

protected:
    CUIArtefactDetectorBase* m_ui;
    bool m_bFastAnimMode;
    bool m_bNeedActivation;

public:
    CCustomDetector();
    virtual ~CCustomDetector();

    virtual bool net_Spawn(CSE_Abstract* DC);
    virtual void Load(LPCSTR section);

    virtual void OnH_A_Chield();
    virtual void OnH_B_Independent(bool just_before_destroy);

    virtual void shedule_Update(u32 dt);
    virtual void UpdateCL();

    bool IsWorking();

    virtual void OnMoveToSlot(const SInvItemPlace& prev);
    virtual void OnMoveToRuck(const SInvItemPlace& prev);

    virtual void OnActiveItem();
    virtual void OnHiddenItem();
    virtual void OnStateSwitch(u32 S, u32 oldState);
    virtual void OnAnimationEnd(u32 state);
    virtual void UpdateXForm();

    void ToggleDetector(bool bFastMode);
    void HideDetector(bool bFastMode);
    void ShowDetector(bool bFastMode);
    float m_fAfDetectRadius;
    virtual bool CheckCompatibility(CHudItem* itm);

    virtual u32 ef_detector_type() const { return 1; };
protected:
    bool CheckCompatibilityInt(CHudItem* itm, u16* slot_to_activate);
    void TurnDetectorInternal(bool b);
    void UpdateNightVisionMode(bool b_off);
    void UpdateVisibility();
    virtual void UpfateWork();
    virtual void UpdateAf(){};
    virtual void CreateUI(){};

    bool m_bWorking;
    float m_fAfVisRadius;
    float m_fDecayRate; //Alundaio
    CAfList m_artefacts;

    // Dead Air: config-driven light + glow particles for placeable detector-class devices
    // (kerosinka lamp, dropped flashlight/glowstick/lighter) via the light_*/particles_* keys
    // already authored on these sections in items_devices.ltx. Nothing in the engine ever read
    // those keys -- CSimpleDetector (the C++ class backing all of them) only lit up the small HUD
    // glow while held (see SimpleDetector.cpp), and the actor's own headlamp light (CTorch) only
    // covers whichever device is in-hand. A device set on the ground has no H_Parent and no
    // handheld light to ride, so it was completely dark. This adds a light + particle effect that
    // lives on the object itself and is only active while it has no parent (i.e. lying in the
    // world), so it never fights with the in-hand behavior. See CCustomDetector::UpdateWorldLight().
    void UpdateWorldLight();
    void StartWorldLight();
    void StopWorldLight();

    bool m_bWorldLightEnabled{false};
    bool m_bWorldLightSpot{false};
    bool m_bWorldLightShadow{false};
    bool m_bWorldLightVolumetric{false};
    float m_fWorldLightRange{0.f};
    float m_fWorldLightBrightness{1.f};
    float m_fWorldLightCone{0.f};
    Fcolor m_WorldLightColor{0.f, 0.f, 0.f, 1.f};
    // Kept parsed (not currently consulted for positioning -- see UpdateWorldLight()) as reference
    // data for whoever revisits the flame-glow work; light_bone is the actor hand-attach point for
    // this model, not the flame, and touching IKinematics to resolve/use a bone here previously
    // reproduced a destroy/recreate churn on this object -- see [[dar3-kerosinka-no-light-on-ground]].
    shared_str m_sWorldLightBoneName;
    CLAItem* m_pWorldLightAnim{nullptr};
    ref_light m_pWorldLight;
    // A visible bright dot at the light source itself -- an IRender_Light only illuminates OTHER
    // surfaces, it draws nothing at its own position, which is why the environment lit up correctly
    // but the lamp's glass looked dark. Same lightweight billboard sprite CTorch/CHangingLamp/CFlare
    // use for this (glow_create()), not a skeleton-driven particle emitter, so it doesn't touch the
    // object's kinematics and hasn't reproduced the destroy/recreate churn particles did. Currently
    // non-functional for an unrelated reason (the engine's glow rendering never actually draws
    // anything in this build) -- see project memory for the full investigation.
    ref_glow m_pWorldGlow;
    // Re-sampled from XFORM() every tick in UpdateWorldLight() (not cached once) -- see the comment
    // there for why "once" doesn't work for this object.
    Fmatrix m_WorldLightXf{};

    bool m_bWorldParticlesEnabled{false};
    shared_str m_sWorldParticlesName;
    // Same "kept for reference, not currently used" status as m_sWorldLightBoneName above.
    shared_str m_sWorldParticlesBoneName;
    CParticlesObject* m_pWorldParticles{nullptr};
};

class CZoneList : public CDetectList<CCustomZone>
{
protected:
    virtual bool feel_touch_contact(IGameObject* O);

public:
    CZoneList();
    virtual ~CZoneList();
}; // class CZoneList
