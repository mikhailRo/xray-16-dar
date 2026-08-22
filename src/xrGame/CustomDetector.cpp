#include "StdAfx.h"
#include "CustomDetector.h"
#include "ui/ArtefactDetectorUI.h"
#include "HUDManager.h"
#include "Inventory.h"
#include "Level.h"
#include "map_manager.h"
#include "ActorEffector.h"
#include "Actor.h"
#include "xrUICore/Windows/UIWindow.h"
#include "player_hud.h"
#include "Weapon.h"
#include "xrEngine/LightAnimLibrary.h"
#include "Include/xrRender/Kinematics.h"
#include "xrPhysics/PhysicsShell.h"

ITEM_INFO::ITEM_INFO() : snd_time(0), cur_period(0)
{
    pParticle = nullptr;
    curr_ref = nullptr;
}

ITEM_INFO::~ITEM_INFO()
{
    if (pParticle)
        CParticlesObject::Destroy(pParticle);
}

bool CCustomDetector::CheckCompatibilityInt(CHudItem* itm, u16* slot_to_activate)
{
    if (itm == nullptr)
        return true;

    CInventoryItem& iitm = itm->item();
    u32 slot = iitm.BaseSlot();
    bool bres = (slot == INV_SLOT_2 || slot == KNIFE_SLOT || slot == BOLT_SLOT || slot == BINOCULAR_SLOT);
    if (!bres && slot_to_activate)
    {
        *slot_to_activate = NO_ACTIVE_SLOT;
        if (m_pInventory->ItemFromSlot(BOLT_SLOT))
            *slot_to_activate = BOLT_SLOT;

        if (m_pInventory->ItemFromSlot(KNIFE_SLOT))
            *slot_to_activate = KNIFE_SLOT;

        if (m_pInventory->ItemFromSlot(INV_SLOT_3) && m_pInventory->ItemFromSlot(INV_SLOT_3)->BaseSlot() != INV_SLOT_3)
            *slot_to_activate = INV_SLOT_3;

        if (m_pInventory->ItemFromSlot(INV_SLOT_2) && m_pInventory->ItemFromSlot(INV_SLOT_2)->BaseSlot() != INV_SLOT_3)
            *slot_to_activate = INV_SLOT_2;

        if (m_pInventory->ItemFromSlot(BINOCULAR_SLOT) && m_pInventory->ItemFromSlot(BINOCULAR_SLOT)->BaseSlot() != INV_SLOT_3)
            *slot_to_activate = BINOCULAR_SLOT;

        if (*slot_to_activate != NO_ACTIVE_SLOT)
            bres = true;
    }

    if (itm->GetState() != CHUDState::eShowing)
        bres = bres && !itm->IsPending();

    if (bres)
    {
        CWeapon* W = smart_cast<CWeapon*>(itm);
        if (W)
            bres = bres && (W->GetState() != CHUDState::eBore) && (W->GetState() != CWeapon::eReload) &&
                (W->GetState() != CWeapon::eSwitch) && !W->IsZoomed();
    }
    return bres;
}

bool CCustomDetector::CheckCompatibility(CHudItem* itm)
{
    if (!inherited::CheckCompatibility(itm))
        return false;

    if (!CheckCompatibilityInt(itm, NULL))
    {
        HideDetector(true);
        return false;
    }
    return true;
}

void CCustomDetector::HideDetector(bool bFastMode)
{
    if (GetState() == eIdle)
        ToggleDetector(bFastMode);
}

void CCustomDetector::ShowDetector(bool bFastMode)
{
    if (GetState() == eHidden)
        ToggleDetector(bFastMode);
}

void CCustomDetector::ToggleDetector(bool bFastMode)
{
    m_bNeedActivation = false;
    m_bFastAnimMode = bFastMode;

    if (GetState() == eHidden)
    {
        PIItem iitem = m_pInventory->ActiveItem();
        CHudItem* itm = (iitem) ? iitem->cast_hud_item() : NULL;
        u16 slot_to_activate = NO_ACTIVE_SLOT;

        if (CheckCompatibilityInt(itm, &slot_to_activate))
        {
            if (slot_to_activate != NO_ACTIVE_SLOT)
            {
                m_pInventory->Activate(slot_to_activate);
                m_bNeedActivation = true;
            }
            else
            {
                SwitchState(eShowing);
                TurnDetectorInternal(true);
            }
        }
    }
    else if (GetState() == eIdle)
        SwitchState(eHiding);
}

void CCustomDetector::OnStateSwitch(u32 S, u32 oldState)
{
    inherited::OnStateSwitch(S, oldState);

    switch (S)
    {
    case eShowing:
    {
        g_player_hud->attach_item(this);
        m_sounds.PlaySound("sndShow", Fvector().set(0, 0, 0), this, true, false);
        PlayHUDMotion(m_bFastAnimMode ? "anm_show_fast" : "anm_show", "anim_show", FALSE /*TRUE*/, this, GetState());
        SetPending(TRUE);
    }
    break;
    case eHiding:
    {
        if (oldState != eHiding)
        {
            m_sounds.PlaySound("sndHide", Fvector().set(0, 0, 0), this, true, false);
            PlayHUDMotion(m_bFastAnimMode ? "anm_hide_fast" : "anm_hide", "anim_show", FALSE/*TRUE*/, this, GetState());
            SetPending(TRUE);
        }
    }
    break;
    case eIdle:
    {
        PlayAnimIdle();
        SetPending(FALSE);
    }
    break;
    }
}

void CCustomDetector::OnAnimationEnd(u32 state)
{
    inherited::OnAnimationEnd(state);
    switch (state)
    {
    case eShowing:
    {
        SwitchState(eIdle);
        if (IsUsingCondition() && m_fDecayRate > 0.f)
            this->SetCondition(-m_fDecayRate);
    }
    break;
    case eHiding:
    {
        SwitchState(eHidden);
        TurnDetectorInternal(false);
        g_player_hud->detach_item(this);
    }
    break;
    }
}

void CCustomDetector::UpdateXForm() { CInventoryItem::UpdateXForm(); }
void CCustomDetector::OnActiveItem() { return; }
void CCustomDetector::OnHiddenItem() {}
CCustomDetector::CCustomDetector()
{
    m_ui = NULL;
    m_bFastAnimMode = false;
    m_bNeedActivation = false;
}

CCustomDetector::~CCustomDetector()
{
    m_artefacts.destroy();
    TurnDetectorInternal(false);
    StopWorldLight();
    xr_delete(m_ui);
}

bool CCustomDetector::net_Spawn(CSE_Abstract* DC)
{
    TurnDetectorInternal(false);
    return (inherited::net_Spawn(DC));
}

void CCustomDetector::Load(LPCSTR section)
{
    m_animation_slot = 7;
    inherited::Load(section);

    m_fAfDetectRadius = pSettings->read_if_exists<float>(section, "af_radius", 30.0f);
    m_fAfVisRadius = pSettings->read_if_exists<float>(section, "af_vis_radius", 2.0f);
    m_fDecayRate = READ_IF_EXISTS(pSettings, r_float, section, "decay_rate", 0.f); //Alundaio
    m_artefacts.load(section, "af");

    m_sounds.LoadSound(section, "snd_draw", "sndShow");
    m_sounds.LoadSound(section, "snd_holster", "sndHide");

    // Dead Air: world light/particles for a placed (parent-less) device -- see the member comment
    // in CustomDetector.h. Only sections that opt in with light_enabled=true pay for any of this;
    // real artefact detectors leave it false (set on the shared detector_fake base) and are untouched.
    m_bWorldLightEnabled = pSettings->read_if_exists<bool>(section, "light_enabled", false);
    if (m_bWorldLightEnabled)
    {
        m_bWorldLightSpot = pSettings->read_if_exists<bool>(section, "light_spot", false);
        m_bWorldLightShadow = pSettings->read_if_exists<bool>(section, "light_shadow", false);
        m_bWorldLightVolumetric = pSettings->read_if_exists<bool>(section, "light_volumetric", false);
        m_fWorldLightRange = pSettings->read_if_exists<float>(section, "light_range", 0.f);
        m_fWorldLightBrightness = pSettings->read_if_exists<float>(section, "light_brightness", 1.f);
        // light_angle is authored as a cone half-angle in radians already (e.g. 0.3, 0.8), the same
        // convention CTorch's baked spot_angle ends up in after its own deg2rad conversion.
        m_fWorldLightCone = pSettings->read_if_exists<float>(section, "light_angle", 0.5f);

        m_WorldLightColor = pSettings->read_if_exists<Fcolor>(section, "light_color", Fcolor(1.f, 1.f, 1.f, 1.f));
        m_WorldLightColor.a = 1.f;
        m_WorldLightColor.mul_rgb(m_fWorldLightBrightness);

        m_sWorldLightBoneName = pSettings->line_exist(section, "light_bone") ? pSettings->r_string(section, "light_bone") : "";

        pcstr anim_name = pSettings->line_exist(section, "light_color_animmator")
            ? pSettings->r_string(section, "light_color_animmator")
            : "";
        m_pWorldLightAnim = (anim_name && anim_name[0] && 0 != xr_stricmp(anim_name, "empty"))
            ? LALib.FindItem(anim_name)
            : nullptr;
    }

    // TEMP diagnostic: forced off while tracking down why the placed object's C++ instance keeps
    // getting destroyed and recreated seconds after spawning -- testing whether the
    // "explosions\effects\..."-named particle group is the trigger (possibly a one-shot FX whose
    // completion something reads as "this object is done, clean it up").
    m_bWorldParticlesEnabled = false;
    (void)pSettings->read_if_exists<bool>(section, "particles_enabled", false);
}

void CCustomDetector::shedule_Update(u32 dt)
{
    inherited::shedule_Update(dt);

    if (!IsWorking())
        return;

    Position().set(H_Parent()->Position());

    Fvector P;
    P.set(H_Parent()->Position());

    if (IsUsingCondition() && GetCondition() <= 0.01f)
        return;

    m_artefacts.feel_touch_update(P, m_fAfDetectRadius);
}

bool CCustomDetector::IsWorking() { return m_bWorking && H_Parent() && H_Parent() == Level().CurrentViewEntity(); }
void CCustomDetector::UpfateWork()
{
    UpdateAf();
    m_ui->update();
}

void CCustomDetector::UpdateVisibility()
{
    // check visibility
    attachable_hud_item* i0 = g_player_hud->attached_item(0);
    if (i0 && HudItemData())
    {
        bool bClimb = ((Actor()->MovingState() & mcClimb) != 0);
        if (bClimb)
        {
            HideDetector(true);
            m_bNeedActivation = true;
        }
        else
        {
            CWeapon* wpn = smart_cast<CWeapon*>(i0->m_parent_hud_item);
            if (wpn)
            {
                u32 state = wpn->GetState();
                if (wpn->IsZoomed() || state == CWeapon::eReload || state == CWeapon::eSwitch)
                {
                    HideDetector(true);
                    m_bNeedActivation = true;
                }
            }
        }
    }
    else if (m_bNeedActivation)
    {
        attachable_hud_item* i0 = g_player_hud->attached_item(0);
        bool bClimb = ((Actor()->MovingState() & mcClimb) != 0);
        if (!bClimb)
        {
            CHudItem* huditem = (i0) ? i0->m_parent_hud_item : NULL;
            bool bChecked = !huditem || CheckCompatibilityInt(huditem, 0);

            if (bChecked)
                ShowDetector(true);
        }
    }
}

void CCustomDetector::UpdateCL()
{
    inherited::UpdateCL();

    UpdateWorldLight();

    if (H_Parent() != Level().CurrentEntity())
        return;

    UpdateVisibility();
    if (!IsWorking())
        return;
    UpfateWork();
}

// Dead Air: see the member comment in CustomDetector.h. Runs every frame regardless of who (if
// anyone) currently controls this object -- UpdateCL fires for any visible object, parented or
// not (CTorch's own dropped-item branch relies on the same fact) -- so a kerosinka lying in the
// world gets its light updated even though nothing "owns" it.
void CCustomDetector::UpdateWorldLight()
{
    if (!m_bWorldLightEnabled && !m_bWorldParticlesEnabled)
        return;

    // Only light the world while lying on the ground. While carried, the actor's own single torch
    // object (CTorch, multiplexed by itms_manager.TorchType) is responsible for lighting whichever
    // device is in-hand -- letting this light run too would double up the illumination and never
    // turn off when the item is holstered.
    //
    // H_Parent(), m_pPhysicsShell, and even CInventoryItem::m_pInventory all turned out unreliable
    // for this on a LOADED SAVE specifically: the [kerosinka-dbg] log showed a second, spare
    // device_kerosinka still sitting in the rucksack passing every one of those checks as "free" and
    // getting its own light, which then tracked the player because its XFORM() is kept in sync with
    // the owner while stashed. m_pInventory is a raw runtime pointer that a save's inventory-restore
    // path apparently doesn't repopulate the same way a fresh runtime pickup does.
    //
    // CInventoryItem::m_ItemCurrPlace, by contrast, IS explicitly part of the save format
    // (inventory_item.cpp: `packet.w_u16(m_ItemCurrPlace.value)` / `r_u16()` in save()/load()), so it
    // reflects the correct placement (eItemPlaceRuck/Slot/Belt) even for save-restored items. Its
    // reset value, eItemPlaceUndefined, is also exactly what a freshly alife-created object (like the
    // one inv_item_place spawns) starts with, since it was never placed into anyone's inventory to
    // begin with -- so this is the one check that's correct for both "just placed" and "restored from
    // a save with a full rucksack".
    if (H_Parent() || CurrPlace() != eItemPlaceUndefined || !getVisible())
    {
        StopWorldLight();
        return;
    }

    // NOTE: deliberately NOT delaying light creation to let the item "settle" first (an earlier
    // version of this code did, and that was wrong): [kerosinka-dbg] logging showed UpdateCL for a
    // just-placed item fires only WHILE its physics body is actively falling/settling, then stops
    // being called at all once it goes to sleep at rest -- a delay-based approach can never see
    // enough ticks to elapse, so the light simply never turns on. Sampling XFORM() every tick instead
    // (below) means the light's position keeps tracking the object for as long as it's still moving,
    // and naturally freezes at the correct final position once physics stops ticking it.

    StartWorldLight();

    // Re-sample every tick this function actually runs (see the NOTE above for why "once" doesn't
    // work): while the object is still falling/settling this keeps the light glued to it, and once
    // physics stops ticking the object, this code simply stops running too, leaving the light at
    // wherever the object's last tick left it -- its final resting position.
    //
    // Deliberately NOT touching IKinematics (CalculateBones()/LL_GetTransform()) here to anchor to a
    // bone, even though that would center the light more precisely on the flame: doing so reintroduced
    // the exact destroy/recreate churn that the particles removal just fixed (same [kerosinka-dbg]
    // STOP reason=destructor pattern, immediately, every time). This object's skeleton is apparently
    // still being actively driven by its own physics while settling, and something about touching it
    // from here at the same time conflicts badly enough that the object gets torn down. XFORM() (the
    // object's root transform) doesn't touch the skeleton at all and has never triggered this.
    m_WorldLightXf = XFORM();
    // Nudge up along the object's own local up-axis to roughly land inside the glass/flame area
    // instead of at the model's base pivot. Started at 0.12 (the flame height documented for
    // bone_lamp in gamedata\configs\models\objects\kerosinka.ltx) but the glow sprite was invisible
    // there -- most likely still inside the lamp's opaque metal base/font, so the depth test hides it
    // (env illumination from the light itself was still fine, since that isn't depth-tested against
    // the lamp's own mesh the same way). Raised to land higher, inside the glass chimney instead.
    m_WorldLightXf.c.mad(m_WorldLightXf.j, 0.2f);

    Fcolor clr = m_WorldLightColor;
    if (m_pWorldLightAnim)
    {
        int frame;
        u32 c = m_pWorldLightAnim->CalculateBGR(Device.fTimeGlobal, frame);
        clr.set((float)color_get_B(c), (float)color_get_G(c), (float)color_get_R(c), 1.f);
        clr.mul_rgb(m_fWorldLightBrightness / 255.f);
    }

    if (m_pWorldLight)
    {
        m_pWorldLight->set_position(m_WorldLightXf.c);
        m_pWorldLight->set_rotation(m_WorldLightXf.k, m_WorldLightXf.i);
        m_pWorldLight->set_color(clr);
    }

    if (m_pWorldGlow)
    {
        m_pWorldGlow->set_position(m_WorldLightXf.c);
        m_pWorldGlow->set_color(clr);
    }

    if (m_pWorldParticles)
        m_pWorldParticles->UpdateParent(m_WorldLightXf, Fvector().set(0.f, 0.f, 0.f));
}

void CCustomDetector::StartWorldLight()
{
    if (m_bWorldLightEnabled && !m_pWorldLight)
    {
        m_pWorldLight = GEnv.Render->light_create();
        m_pWorldLight->set_type(m_bWorldLightSpot ? IRender_Light::SPOT : IRender_Light::POINT);
        // Deliberately ignoring m_bWorldLightShadow (light_shadow=true in the ltx) here: this
        // engine's shadow-map path is only reliable for a light whose position is set once and never
        // again ("static lamps"). This light gets set_position/set_rotation called every frame in
        // UpdateWorldLight() even though the value barely changes for a placed item, and that alone
        // was enough to reproduce the exact bug CTorch already worked around with this same call
        // (see the long comment in Torch.cpp's constructor / project memory
        // dar3-dynamic-lights-dont-light-environment): a shadow-mapped light re-evaluated every frame
        // breaks accum_spot/accum_point's stencil marking and the light leaks/mispositions instead of
        // properly illuminating+occluding. Forcing it off fixed a bright mispositioned patch that
        // showed up next to (not on) the placed lamp.
        m_pWorldLight->set_shadow(false);
        m_pWorldLight->set_volumetric(m_bWorldLightVolumetric);
        m_pWorldLight->set_range(m_fWorldLightRange);
        if (m_bWorldLightSpot)
            m_pWorldLight->set_cone(m_fWorldLightCone);

        // Set an initial position here too (UpdateWorldLight() re-samples every tick right after this
        // returns, so this is mostly just "don't leave it at the origin for one frame").
        m_WorldLightXf = XFORM();
        m_pWorldLight->set_position(m_WorldLightXf.c);
        m_pWorldLight->set_rotation(m_WorldLightXf.k, m_WorldLightXf.i);

        m_pWorldLight->set_active(true);

        // Visible flame dot -- see the member comment on m_pWorldGlow. "fire1" reads as a warm open
        // flame, matching kerosinka's amber light_color much better than the flashlight/torch glow.
        m_pWorldGlow = GEnv.Render->glow_create();
        m_pWorldGlow->set_texture("glow\\glow_fire1");
        m_pWorldGlow->set_position(m_WorldLightXf.c);
        m_pWorldGlow->set_color(m_WorldLightColor);
        m_pWorldGlow->set_radius(0.25f);
        m_pWorldGlow->set_active(true);
    }

    if (m_bWorldParticlesEnabled && !m_pWorldParticles)
    {
        m_pWorldParticles = CParticlesObject::Create(m_sWorldParticlesName.c_str(), FALSE);
        m_pWorldParticles->Play(true);
    }
}

void CCustomDetector::StopWorldLight()
{
    if (m_pWorldLight)
        m_pWorldLight.destroy();

    if (m_pWorldGlow)
        m_pWorldGlow.destroy();

    if (m_pWorldParticles)
        CParticlesObject::Destroy(m_pWorldParticles);
}

// Dead Air: don't rely on UpdateWorldLight()'s per-frame ownership check alone to turn the world
// light back off on pickup -- UpdateCL apparently stops being driven reliably once the object is no
// longer rendered as a free-standing item (stashed/equipped), which left the light created in
// StartWorldLight() stuck active at its last position forever (looked like a stray light "floating"
// nearby, since a now-orphaned static point light only ever appears to move via normal camera
// parallax). OnH_A_Chield/OnMoveToRuck/OnMoveToSlot are the engine's own explicit "this item just
// stopped being a free object" hooks, so stop the light there instead of hoping a future frame update
// gets the chance to notice.
void CCustomDetector::OnH_A_Chield()
{
    inherited::OnH_A_Chield();
    StopWorldLight();
}

void CCustomDetector::OnH_B_Independent(bool just_before_destroy)
{
    inherited::OnH_B_Independent(just_before_destroy);

    m_artefacts.clear();

	if (GetState() != eHidden)
	{
		// Detaching hud item and animation stop in OnH_A_Independent
		TurnDetectorInternal(false);
		SwitchState(eHidden);
	}
}

void CCustomDetector::OnMoveToRuck(const SInvItemPlace& prev)
{
    inherited::OnMoveToRuck(prev);
    if (prev.type == eItemPlaceSlot)
    {
        SwitchState(eHidden);
        g_player_hud->detach_item(this);
    }
    TurnDetectorInternal(false);
    StopCurrentAnimWithoutCallback();
    StopWorldLight();
}

void CCustomDetector::OnMoveToSlot(const SInvItemPlace& prev)
{
    inherited::OnMoveToSlot(prev);
    StopWorldLight();
}
void CCustomDetector::TurnDetectorInternal(bool b)
{
    m_bWorking = b;
    if (b && m_ui == NULL)
    {
        CreateUI();
    }
    else
    {
        //.		xr_delete			(m_ui);
    }

    UpdateNightVisionMode(b);
}

#include "game_base_space.h"
void CCustomDetector::UpdateNightVisionMode(bool b_on) {}
bool CAfList::feel_touch_contact(IGameObject* O)
{
    TypesMapIt it = m_TypesMap.find(O->cNameSect());

    bool res = (it != m_TypesMap.end());
    if (res)
    {
        CArtefact* pAf = smart_cast<CArtefact*>(O);

        if (pAf->GetAfRank() > m_af_rank)
            res = false;
    }
    return res;
}
