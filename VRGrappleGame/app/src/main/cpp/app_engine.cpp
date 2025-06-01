#include "util_egl.h"
#include "util_oxr.h"
#include "app_engine.h"
#include "render_scene.h"
#include "VRGame.h"
#include "Oculus_OpenXR_Mobile_SDK/SampleCommon/Src/Misc/Log.h"


#include <android/log.h>

#ifndef APP_TAG
#define APP_TAG "GlassGrapple"  // Your custom tag for filtering
#endif


AppEngine::AppEngine (android_app* app)
    : m_app(app)
{
}

AppEngine::~AppEngine()
{
}


/* ---------------------------------------------------------------------------- *
 *  Interfaces to android application framework
 * ---------------------------------------------------------------------------- */
struct android_app *
AppEngine::AndroidApp (void) const
{
    return m_app;
}


/* ---------------------------------------------------------------------------- *
 *  Initialize OpenXR with OpenGLES renderer
 * ---------------------------------------------------------------------------- */
void AppEngine::InitOpenXR_GLES ()
{
    __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "Step 1 in INIT");

    void *vm    = m_app->activity->vm;
    void *clazz = m_app->activity->clazz;

    oxr_initialize_loader (vm, clazz);


    __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "Step 2 in INIT");

    m_instance = oxr_create_instance (vm, clazz);
    m_systemId = oxr_get_system (m_instance);

    egl_init_with_pbuffer_surface (3, 24, 0, 0, 16, 16);
    oxr_confirm_gfx_requirements (m_instance, m_systemId);

    init_gles_scene ();
    __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "Step 3 in INIT");

    m_session    = oxr_create_session (m_instance, m_systemId);



    // Load the XR_FB_display_refresh_rate function pointers
    PFN_xrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB = nullptr;
    PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB = nullptr;

    xrGetInstanceProcAddr(m_instance, "xrEnumerateDisplayRefreshRatesFB",
                          (PFN_xrVoidFunction*)&xrEnumerateDisplayRefreshRatesFB);
    xrGetInstanceProcAddr(m_instance, "xrRequestDisplayRefreshRateFB",
                          (PFN_xrVoidFunction*)&xrRequestDisplayRefreshRateFB);
    __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "Step 4 in INIT");

    if (xrEnumerateDisplayRefreshRatesFB && xrRequestDisplayRefreshRateFB) {
        uint32_t rateCount = 0;
        xrEnumerateDisplayRefreshRatesFB(m_session, 0, &rateCount, nullptr);
        __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "📊 Available refresh rate count: %u", rateCount);

        if (rateCount > 0) {
            std::vector<float> rates(rateCount);
            xrEnumerateDisplayRefreshRatesFB(m_session, rateCount, &rateCount, rates.data());

            for (float rate : rates) {
                __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "📈 Available refresh rate: %.1f Hz", rate);

                if (fabs(rate - 120.0f) < 0.1f) {
                    XrResult result = xrRequestDisplayRefreshRateFB(m_session, 120.0f);
                    if (result == XR_SUCCESS) {
                        __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "✅ Successfully requested 120 Hz refresh rate");
                    } else {
                        __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "⚠️ Failed to request 120 Hz: %d", result);
                    }
                }
            }
        }
    } else {
        __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "❌ Refresh rate extension functions not found.");
    }
        __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "🎯 Requested 120Hz refresh rate");




    m_appSpace   = oxr_create_ref_space (m_session, XR_REFERENCE_SPACE_TYPE_LOCAL);
    m_stageSpace = oxr_create_ref_space (m_session, XR_REFERENCE_SPACE_TYPE_STAGE);
    m_viewSpace  = oxr_create_ref_space (m_session, XR_REFERENCE_SPACE_TYPE_VIEW);

    m_viewSurface = oxr_create_viewsurface (m_instance, m_systemId, m_session);
    __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "Step 5 in INIT");

    InitializeActions ();

//    oxr_create_handtrackers (m_instance, m_session, m_handTracker);
//    m_handJointLoc[0] = oxr_create_handjoint_loc ();
//    m_handJointLoc[1] = oxr_create_handjoint_loc ();

    m_runtime_name = oxr_get_runtime_name (m_instance);
    m_system_name  = oxr_get_system_name (m_instance, m_systemId);

    AAssetManager* mgr = m_app->activity->assetManager;
    __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "Step 6 in INIT");

//    copyAllMp3Assets(mgr, "com.DRHudooken.GlassGrapple");  // <-- Set the package name to find the assets: SET THIS
    copyAllAssets(mgr, "com.DRHudooken.GlassGrapple");

}


/* ---------------------------------------------------------------------------- *
 *  Initialize Hand Controller Action
 * ---------------------------------------------------------------------------- *
 *
 * -----------+-----------+-----------------+-------------------------------------
 *  XrAction  | ActionType|  SubactionPath  | BindingPath
 * -----------+-----------+-----------------+-------------------------------------
 *  poseAction| POSE   IN | /user/hand/left | /user/hand/left /input/grip/pose
 *            |           | /user/hand/right| /user/hand/right/input/grip/pose
 *  aimAction | POSE   IN | /user/hand/left | /user/hand/left /input/aim/pose
 *            |           | /user/hand/right| /user/hand/right/input/aim/pose
 *  squzAction| FLOAT  IN | /user/hand/left | /user/hand/left /input/squeeze/value
 *            |           | /user/hand/right| /user/hand/right/input/squeeze/value
 *  trigAction| FLOAT  IN | /user/hand/left | /user/hand/left /input/trigger/value
 *            |           | /user/hand/right| /user/hand/right/input/trigger/value
 *  stikAction| FLOAT  IN | /user/hand/left | /user/hand/left /input/trigger/value
 *            |           | /user/hand/right| /user/hand/right/input/trigger/value
 * -----------+-----------+-----------------+-------------------------------------
 *  vibrAction|VIBRATE OUT| /user/hand/left | /user/hand/left /output/haptic
 *            |           | /user/hand/right| /user/hand/right/output/haptic
 * -----------+-----------+-----------------+-------------------------------------
 *  clksAction| BOOL   IN | /user/hand/left | /user/hand/left /input/thumbstick/click
 *  clksAction| BOOL   IN | /user/hand/right| /user/hand/right/input/thumbstick/click
 *  clkaAction| BOOL   IN |                 | /user/hand/right/input/a/click
 *  clkbAction| BOOL   IN |                 | /user/hand/right/input/b/click
 *  clkxAction| BOOL   IN |                 | /user/hand/left /input/x/click
 *  clkyAction| BOOL   IN |                 | /user/hand/left /input/y/click
 *  quitAction| BOOL   IN |                 | /user/hand/left /input/menu/click
 * -----------+-----------+-----------------+-------------------------------------
 *
 */

#define HANDL       "/user/hand/left"
#define HANDR       "/user/hand/right"
#define HANDL_IN    "/user/hand/left/input"
#define HANDR_IN    "/user/hand/right/input"
#define HANDL_OT    "/user/hand/left/output"
#define HANDR_OT    "/user/hand/right/output"

void
AppEngine::InitializeActions()
{
    m_input.actionSet = oxr_create_actionset (m_instance, "app_action_set", "AppActionSet", 0);

    xrStringToPath (m_instance, HANDL, &m_input.handSubactionPath[Side::LEFT ]);
    xrStringToPath (m_instance, HANDR, &m_input.handSubactionPath[Side::RIGHT]);

    XrActionSet           &actSet  = m_input.actionSet;
    std::array<XrPath, 2> &subPath = m_input.handSubactionPath;

    // Create actions.
    m_input.poseAction = oxr_create_action (actSet, XR_ACTION_TYPE_POSE_INPUT,       "grip_pose","GripPose", 2, subPath.data());
    m_input.aimAction  = oxr_create_action (actSet, XR_ACTION_TYPE_POSE_INPUT,       "aim_pose", "Aim Pose", 2, subPath.data());
    m_input.squzAction = oxr_create_action (actSet, XR_ACTION_TYPE_FLOAT_INPUT,      "squeeze",  "Aqueeze",  2, subPath.data());
    m_input.trigAction = oxr_create_action (actSet, XR_ACTION_TYPE_FLOAT_INPUT,      "trigger",  "Trigger",  2, subPath.data());
    m_input.stikAction = oxr_create_action (actSet, XR_ACTION_TYPE_VECTOR2F_INPUT,   "thumstick","ThumStick",2, subPath.data());
    m_input.vibrAction = oxr_create_action (actSet, XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic",   "Haptic",   2, subPath.data());
    m_input.clisAction = oxr_create_action (actSet, XR_ACTION_TYPE_BOOLEAN_INPUT,    "click_s",  "ClickS",   2, subPath.data());
    m_input.cliaAction = oxr_create_action (actSet, XR_ACTION_TYPE_BOOLEAN_INPUT,    "click_a",  "ClickA",   0, NULL);
    m_input.clibAction = oxr_create_action (actSet, XR_ACTION_TYPE_BOOLEAN_INPUT,    "click_b",  "ClickB",   0, NULL);
    m_input.clixAction = oxr_create_action (actSet, XR_ACTION_TYPE_BOOLEAN_INPUT,    "click_x",  "ClickX",   0, NULL);
    m_input.cliyAction = oxr_create_action (actSet, XR_ACTION_TYPE_BOOLEAN_INPUT,    "click_y",  "ClickY",   0, NULL);
    m_input.quitAction = oxr_create_action (actSet, XR_ACTION_TYPE_BOOLEAN_INPUT,    "menu_quit","MenuQuit", 0, NULL);

    // Suggest bindings for the Oculus Touch.
    {
        std::vector<XrActionSuggestedBinding> bindings;
        bindings.push_back ({m_input.poseAction, oxr_str2path (m_instance, HANDL_IN"/grip/pose"    )});
        bindings.push_back ({m_input.poseAction, oxr_str2path (m_instance, HANDR_IN"/grip/pose"    )});
        bindings.push_back ({m_input.aimAction,  oxr_str2path (m_instance, HANDL_IN"/aim/pose"     )});
        bindings.push_back ({m_input.aimAction,  oxr_str2path (m_instance, HANDR_IN"/aim/pose"     )});
        bindings.push_back ({m_input.squzAction, oxr_str2path (m_instance, HANDL_IN"/squeeze/value")});
        bindings.push_back ({m_input.squzAction, oxr_str2path (m_instance, HANDR_IN"/squeeze/value")});
        bindings.push_back ({m_input.trigAction, oxr_str2path (m_instance, HANDL_IN"/trigger/value")});
        bindings.push_back ({m_input.trigAction, oxr_str2path (m_instance, HANDR_IN"/trigger/value")});
        bindings.push_back ({m_input.stikAction, oxr_str2path (m_instance, HANDL_IN"/thumbstick"   )});
        bindings.push_back ({m_input.stikAction, oxr_str2path (m_instance, HANDR_IN"/thumbstick"   )});
        bindings.push_back ({m_input.vibrAction, oxr_str2path (m_instance, HANDL_OT"/haptic"       )});
        bindings.push_back ({m_input.vibrAction, oxr_str2path (m_instance, HANDR_OT"/haptic"       )});
        bindings.push_back ({m_input.clisAction, oxr_str2path (m_instance, HANDR_IN"/thumbstick/click")});
        bindings.push_back ({m_input.clisAction, oxr_str2path (m_instance, HANDL_IN"/thumbstick/click")});
        bindings.push_back ({m_input.cliaAction, oxr_str2path (m_instance, HANDR_IN"/a/click"      )});
        bindings.push_back ({m_input.clibAction, oxr_str2path (m_instance, HANDR_IN"/b/click"      )});
        bindings.push_back ({m_input.clixAction, oxr_str2path (m_instance, HANDL_IN"/x/click"      )});
        bindings.push_back ({m_input.cliyAction, oxr_str2path (m_instance, HANDL_IN"/y/click"      )});
        bindings.push_back ({m_input.quitAction, oxr_str2path (m_instance, HANDL_IN"/menu/click"   )});

        oxr_bind_interaction (m_instance, "/interaction_profiles/oculus/touch_controller", bindings);
    }

    /* attach actions bound to the device. */
    oxr_attach_actionsets (m_session, m_input.actionSet);

    /* create ActionSpace */
    m_input.handSpace[Side::LEFT ] = oxr_create_action_space (m_session, m_input.poseAction, subPath[Side::LEFT ]);
    m_input.handSpace[Side::RIGHT] = oxr_create_action_space (m_session, m_input.poseAction, subPath[Side::RIGHT]);
    m_input.aimSpace [Side::LEFT ] = oxr_create_action_space (m_session, m_input.aimAction,  subPath[Side::LEFT ]);
    m_input.aimSpace [Side::RIGHT] = oxr_create_action_space (m_session, m_input.aimAction,  subPath[Side::RIGHT]);
}


void 
AppEngine::PollActions()
{
    oxr_sync_actions (m_session, m_input.actionSet);

    m_input.handActive = {XR_FALSE, XR_FALSE};

    // Get Pose and Squeeze action state and start Haptic vibrate when hand is 90% squeezed.
    for (auto hand : {Side::LEFT, Side::RIGHT})
    {
        XrPath subPath = m_input.handSubactionPath[hand];

        /* Squeeze */
        {
            XrActionStateFloat stat = oxr_get_action_state_float (m_session, m_input.squzAction, subPath);
            if (stat.isActive == XR_TRUE)
            {
                m_input.squeezeVal[hand] = stat.currentState;
                if (stat.currentState > 0.9f)
                {
                    oxr_apply_haptic_feedback_vibrate (m_session, m_input.vibrAction, subPath,
                        XR_MIN_HAPTIC_DURATION, XR_FREQUENCY_UNSPECIFIED, 0.5f);
                }
            }
        }
        /* Trigger */
        {
            XrActionStateFloat stat = oxr_get_action_state_float (m_session, m_input.trigAction, subPath);
            if (stat.isActive == XR_TRUE)
                m_input.triggerVal[hand] = stat.currentState;
        }
        /* ThumbStick */
        {
            XrActionStateVector2f stat = oxr_get_action_state_vector2 (m_session, m_input.stikAction, subPath);
            if (stat.isActive == XR_TRUE)
                m_input.stickVal[hand] = stat.currentState;
        }
        /* Button-S (ThumbStick Click) */
        {
            XrActionStateBoolean stat = oxr_get_action_state_boolean (m_session, m_input.clisAction, subPath);
            if (stat.isActive == XR_TRUE)
                m_input.clickS[hand] = stat.currentState;
        }
        /* Pose */
        {
            XrActionStatePose stat = oxr_get_action_state_pose (m_session, m_input.poseAction, subPath);
            m_input.handActive[hand] = stat.isActive;
        }
    }

    /* Button-A */
    {
        XrActionStateBoolean stat = oxr_get_action_state_boolean (m_session, m_input.cliaAction, 0);
        if (stat.isActive == XR_TRUE)
            m_input.clickA = stat.currentState;
    }
    /* Button-B */
    {
        XrActionStateBoolean stat = oxr_get_action_state_boolean (m_session, m_input.clibAction, 0);
        if (stat.isActive == XR_TRUE)
            m_input.clickB = stat.currentState;
    }
    /* Button-X */
    {
        XrActionStateBoolean stat = oxr_get_action_state_boolean (m_session, m_input.clixAction, 0);
        if (stat.isActive == XR_TRUE)
            m_input.clickX = stat.currentState;
    }
    /* Button-Y */
    {
        XrActionStateBoolean stat = oxr_get_action_state_boolean (m_session, m_input.cliyAction, 0);
        if (stat.isActive == XR_TRUE)
            m_input.clickY = stat.currentState;
    }
    /* Button-Menu */
    {
        XrActionStateBoolean stat = oxr_get_action_state_boolean (m_session, m_input.quitAction, 0);
        if ((stat.isActive             == XR_TRUE) &&
            (stat.changedSinceLastSync == XR_TRUE) &&
            (stat.currentState         == XR_TRUE))
        {
            xrRequestExitSession (m_session);
            LOGI ("----------- xrRequestExitSession() --------");
        }
    }
}


/* ------------------------------------------------------------------------------------ *
 *  Update  Frame (Event handle, Render)
 * ------------------------------------------------------------------------------------ */
void
AppEngine::UpdateFrame()
{
    bool exit_loop, req_restart;
    oxr_poll_events (m_instance, m_session, &exit_loop, &req_restart);

    if (!oxr_is_session_running())
    {
        return;
    }

    PollActions();




    RenderFrame();
}


/* ------------------------------------------------------------------------------------ *
 *  RenderFrame (Frame/Layer/View)
 * ------------------------------------------------------------------------------------ */
void AppEngine::RenderFrame()
{
    std::vector<XrCompositionLayerBaseHeader*> all_layers;

    XrTime dpy_time, elapsed_us;
    oxr_begin_frame(m_session, &dpy_time);

    //testing other code to log errors better:
    static int frameCount = 0;
    static bool refreshRequested = false;

    frameCount++;

    if (!refreshRequested && frameCount >= 100) {
        PFN_xrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB = nullptr;
        PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB = nullptr;
        PFN_xrResultToString xrResultToString = nullptr;

        xrGetInstanceProcAddr(m_instance, "xrEnumerateDisplayRefreshRatesFB", (PFN_xrVoidFunction*)&xrEnumerateDisplayRefreshRatesFB);
        xrGetInstanceProcAddr(m_instance, "xrRequestDisplayRefreshRateFB", (PFN_xrVoidFunction*)&xrRequestDisplayRefreshRateFB);
        xrGetInstanceProcAddr(m_instance, "xrResultToString", (PFN_xrVoidFunction*)&xrResultToString);

        char resultStr[XR_MAX_RESULT_STRING_SIZE] = {0};

        if (xrEnumerateDisplayRefreshRatesFB && xrRequestDisplayRefreshRateFB && xrResultToString) {
            uint32_t count = 0;
            XrResult countResult = xrEnumerateDisplayRefreshRatesFB(m_session, 0, &count, nullptr);
            xrResultToString(m_instance, countResult, resultStr);
            __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "📊 Rate count result: %s (%d), count = %u", resultStr, countResult, count);

            if (count > 0) {
                std::vector<float> rates(count);
                XrResult listResult = xrEnumerateDisplayRefreshRatesFB(m_session, count, &count, rates.data());
                xrResultToString(m_instance, listResult, resultStr);
                __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "📊 Rate list result: %s (%d)", resultStr, listResult);

                for (float rate : rates) {
                    __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "📈 Available refresh rate: %.1f Hz", rate);
                }
            }

            // ✅ Always try 120Hz request even if not enumerated
            XrResult reqResult = xrRequestDisplayRefreshRateFB(m_session, 120.0f);
            xrResultToString(m_instance, reqResult, resultStr);
            __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "🎯 120Hz request result: %s (%d)", resultStr, reqResult);
        } else {
            __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "❌ Could not get function pointers for FB_refresh_rate or resultToString");
        }

        refreshRequested = true;
    }















    //my code for seeing if 120hz is available after 5 frames
//    static int frameCount = 0;
//    static bool refreshRequested = false;
//
//    frameCount++;
//
//    if (!refreshRequested && frameCount >= 5) {
//        PFN_xrEnumerateDisplayRefreshRatesFB xrEnumerateDisplayRefreshRatesFB = nullptr;
//        PFN_xrRequestDisplayRefreshRateFB xrRequestDisplayRefreshRateFB = nullptr;
//
//        xrGetInstanceProcAddr(m_instance, "xrEnumerateDisplayRefreshRatesFB",
//                              (PFN_xrVoidFunction*)&xrEnumerateDisplayRefreshRatesFB);
//        xrGetInstanceProcAddr(m_instance, "xrRequestDisplayRefreshRateFB",
//                              (PFN_xrVoidFunction*)&xrRequestDisplayRefreshRateFB);
//
//
//        XrResult result = xrRequestDisplayRefreshRateFB(m_session, 120.0f);
//
//        char resultStr[XR_MAX_RESULT_STRING_SIZE] = {0};
//        xrResultToString(m_instance, result, resultStr);
//
//        __android_log_print(ANDROID_LOG_INFO, "GlassGrapple",
//                            "🎯 120Hz request result: %s (%d)", resultStr, result);
//
//
//        if (xrEnumerateDisplayRefreshRatesFB && xrRequestDisplayRefreshRateFB) {
//            uint32_t count = 0;
//            XrResult countResult = xrEnumerateDisplayRefreshRatesFB(m_session, 0, &count, nullptr);
//            xrResultToString(m_instance, countResult, resultStr);
//            __android_log_print(ANDROID_LOG_INFO, "GlassGrapple",
//                                "📊 Step 1: Get rate count → %s (%d), count=%u", resultStr, countResult, count);
//
//            std::vector<float> rates(count);
//            xrEnumerateDisplayRefreshRatesFB(m_session, count, &count, rates.data());
//
//            for (float rate : rates) {
//                __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "📈 Delayed Available refresh rate: %.1f Hz", rate);
//                if (fabs(rate - 120.0f) < 0.1f) {
//                    xrRequestDisplayRefreshRateFB(m_session, 120.0f);
//                    if (result == XR_SUCCESS) {
//                        __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "✅ Delayed 120 Hz set (delayed)");
//                    } else {
//                        __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "❌ Delayed Failed to request 120 Hz: %d", result);
//                    }
//                    break;
//                }
//            }
//        }
//        __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "🎯 Delayed Requested 120Hz refresh rate");
//
//        refreshRequested = true;
//    }
//    else{
//        __android_log_print(ANDROID_LOG_INFO, "GlassGrapple", "📈 Waiting for frames before refresh: %d", frameCount);
//
//    }








    static XrTime init_time = -1;
    if (init_time < 0)
        init_time = dpy_time;
    elapsed_us = (dpy_time - init_time) / 1000;

    std::vector<XrCompositionLayerProjectionView> projLayerViews;
    XrCompositionLayerProjection                  projLayer;
    RenderLayer (dpy_time, elapsed_us, projLayerViews, projLayer);

    all_layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&projLayer));

    /* Compose all layers */
    oxr_end_frame (m_session, dpy_time, all_layers);
}

bool
AppEngine::RenderLayer(XrTime dpy_time,
                       XrTime elapsed_us,
                       std::vector<XrCompositionLayerProjectionView> &layerViews,
                       XrCompositionLayerProjection                  &layer)
{
    /* Acquire View Location */
    uint32_t viewCount = (uint32_t)m_viewSurface.size();

    std::vector<XrView> views(viewCount, {XR_TYPE_VIEW});
    oxr_locate_views (m_session, dpy_time, m_stageSpace, &viewCount, views.data());

    layerViews.resize (viewCount);

    /* Acquire Stage Location, View Location (rerative to the View Location) */
    XrSpaceLocation stageLoc {XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation viewLoc  {XR_TYPE_SPACE_LOCATION};
    xrLocateSpace (m_stageSpace, m_stageSpace, dpy_time, &stageLoc);
    xrLocateSpace (m_viewSpace,  m_stageSpace, dpy_time, &viewLoc);


    /* Acquire Hand-Space Location (rerative to the View Location) */
    std::array<XrSpaceLocation, 2> handLoc;
    std::array<XrSpaceLocation, 2> aimLoc;
    for (uint32_t i = 0; i < 2; i ++)
    {
        handLoc[i] = {XR_TYPE_SPACE_LOCATION};
        xrLocateSpace (m_input.handSpace[i], m_stageSpace, dpy_time, &handLoc[i]);

        aimLoc[i]  = {XR_TYPE_SPACE_LOCATION};
        xrLocateSpace (m_input.aimSpace[i],  m_stageSpace, dpy_time, &aimLoc[i]);
    }

//    oxr_locate_handjoints (m_instance, m_handTracker[0], m_stageSpace, dpy_time, m_handJointLoc[0]);
//    oxr_locate_handjoints (m_instance, m_handTracker[1], m_stageSpace, dpy_time, m_handJointLoc[1]);

    /* Render each view */
    for (uint32_t i = 0; i < viewCount; i++) {
        XrSwapchainSubImage subImg;
        render_target_t     rtarget;

        oxr_acquire_viewsurface (m_viewSurface[i], rtarget, subImg);

        layerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
        layerViews[i].pose     = views[i].pose;
        layerViews[i].fov      = views[i].fov;
        layerViews[i].subImage = subImg;

        scene_data_t sceneData;
        sceneData.runtime_name  = m_runtime_name;
        sceneData.system_name   = m_system_name;
        sceneData.elapsed_us    = elapsed_us;
        sceneData.viewID        = i;
        sceneData.views         = views;
        sceneData.handLoc       = handLoc;
        sceneData.aimLoc        = aimLoc;
        sceneData.inputState    = m_input;
//        sceneData.handJointLoc  = m_handJointLoc;
        render_gles_scene (layerViews[i], rtarget, viewLoc.pose, stageLoc.pose, sceneData);

        oxr_release_viewsurface (m_viewSurface[i]);
    }
    layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
    layer.space     = m_stageSpace;
    layer.viewCount = (uint32_t)layerViews.size();
    layer.views     = layerViews.data();

    return true;
}

