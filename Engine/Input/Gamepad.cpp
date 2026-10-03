#include "Gamepad.h"
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <dinput.h>
#include <xinput.h>
#ifndef DIDFT_OPTIONAL
#define DIDFT_OPTIONAL 0x80000000
#endif

namespace
{
    // GUID de DirectInput définis ici : pas besoin de lier dxguid / dinput8
    const GUID kIID_IDirectInput8A = {0xBF798030, 0x483A, 0x4DA2, {0xAA, 0x99, 0x5D, 0x64, 0xED, 0x36, 0x97, 0x00}};
#define LARVA_AXIS_GUID(d1) {d1, 0xC9F3, 0x11CF, {0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}}
    const GUID kXAxis = LARVA_AXIS_GUID(0xA36D02E0), kYAxis = LARVA_AXIS_GUID(0xA36D02E1), kZAxis = LARVA_AXIS_GUID(0xA36D02E2);
    const GUID kRxAxis = LARVA_AXIS_GUID(0xA36D02F4), kRyAxis = LARVA_AXIS_GUID(0xA36D02F5), kRzAxis = LARVA_AXIS_GUID(0xA36D02E3);
    const GUID kSlider = LARVA_AXIS_GUID(0xA36D02E4), kPOV = LARVA_AXIS_GUID(0xA36D02F2);
#undef LARVA_AXIS_GUID
    const GUID kConstantForce = {0x13541C20, 0x8E33, 0x11D0, {0x9A, 0xD0, 0x00, 0xA0, 0xC9, 0xA0, 0x6E, 0x35}};

    typedef HRESULT(WINAPI *DirectInput8CreateFn)(HINSTANCE, DWORD, REFIID, LPVOID *, LPUNKNOWN);
    typedef DWORD(WINAPI *XInputGetStateFn)(DWORD, XINPUT_STATE *);
    typedef DWORD(WINAPI *XInputSetStateFn)(DWORD, XINPUT_VIBRATION *);

    struct Device
    {
        Gamepad::DeviceInfo info;
        Gamepad::State state;
        int xinputSlot = -1;
        IDirectInputDevice8A *di = nullptr;
        IDirectInputEffect *effect = nullptr;
        GUID instance{};
        LONG lastForce = 0;
        float rumbleLow = -1.0f, rumbleHigh = -1.0f;
    };

    HWND window = nullptr;
    HMODULE dinputDll = nullptr, xinputDll = nullptr;
    IDirectInput8A *directInput = nullptr;
    XInputGetStateFn xGetState = nullptr;
    XInputSetStateFn xSetState = nullptr;
    std::vector<Device> devices;
    DWORD lastXInputScan = 0;
    bool xConnected[4] = {};

    // Format de données DIJOYSTATE (équivalent de c_dfDIJoystick, sans dinput8.lib)
    DIOBJECTDATAFORMAT joyObjects[44];
    DIDATAFORMAT joyFormat;

    void buildFormat()
    {
        int k = 0;
        auto add = [&](const GUID *g, DWORD offset, DWORD type)
        {
            joyObjects[k].pguid = g;
            joyObjects[k].dwOfs = offset;
            joyObjects[k].dwType = DIDFT_OPTIONAL | type | DIDFT_ANYINSTANCE;
            joyObjects[k].dwFlags = 0;
            ++k;
        };
        add(&kXAxis, 0, DIDFT_AXIS);
        add(&kYAxis, 4, DIDFT_AXIS);
        add(&kZAxis, 8, DIDFT_AXIS);
        add(&kRxAxis, 12, DIDFT_AXIS);
        add(&kRyAxis, 16, DIDFT_AXIS);
        add(&kRzAxis, 20, DIDFT_AXIS);
        add(&kSlider, 24, DIDFT_AXIS);
        add(&kSlider, 28, DIDFT_AXIS);
        for (int i = 0; i < 4; ++i)
            add(&kPOV, 32 + 4 * i, DIDFT_POV);
        for (int i = 0; i < 32; ++i)
            add(nullptr, 48 + i, DIDFT_BUTTON);
        joyFormat.dwSize = sizeof(DIDATAFORMAT);
        joyFormat.dwObjSize = sizeof(DIOBJECTDATAFORMAT);
        joyFormat.dwFlags = DIDF_ABSAXIS;
        joyFormat.dwDataSize = sizeof(DIJOYSTATE);
        joyFormat.dwNumObjs = k;
        joyFormat.rgodf = joyObjects;
    }

    // VID/PID des manettes XInput (leur nom de périphérique brut contient "IG_") : on ne les ouvre pas
    // une deuxième fois par DirectInput
    std::vector<DWORD> xinputProducts()
    {
        std::vector<DWORD> ids;
        UINT count = 0;
        if (GetRawInputDeviceList(nullptr, &count, sizeof(RAWINPUTDEVICELIST)) != 0 || count == 0)
            return ids;
        std::vector<RAWINPUTDEVICELIST> list(count);
        if (GetRawInputDeviceList(list.data(), &count, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1)
            return ids;
        for (UINT i = 0; i < count; ++i)
        {
            char name[512] = {};
            UINT size = sizeof(name);
            if (GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICENAME, name, &size) == (UINT)-1)
                continue;
            for (char &c : name)
                c = (char)toupper((unsigned char)c);
            const char *v = strstr(name, "VID_"), *p = strstr(name, "PID_");
            if (!strstr(name, "IG_") || !v || !p)
                continue;
            DWORD vid = strtoul(v + 4, nullptr, 16), pid = strtoul(p + 4, nullptr, 16);
            ids.push_back(MAKELONG(vid, pid));
        }
        return ids;
    }

    struct EnumContext
    {
        std::vector<DIDEVICEINSTANCEA> found;
    };

    BOOL CALLBACK enumDevice(LPCDIDEVICEINSTANCEA inst, LPVOID ctx)
    {
        static_cast<EnumContext *>(ctx)->found.push_back(*inst);
        return DIENUM_CONTINUE;
    }

    void releaseDevice(Device &d)
    {
        if (d.effect)
        {
            d.effect->Stop();
            d.effect->Release();
            d.effect = nullptr;
        }
        if (d.di)
        {
            d.di->Unacquire();
            d.di->Release();
            d.di = nullptr;
        }
    }

    bool openDirectInput(Device &d, const DIDEVICEINSTANCEA &inst)
    {
        IDirectInputDevice8A *dev = nullptr;
        if (FAILED(directInput->CreateDevice(inst.guidInstance, &dev, nullptr)) || !dev)
            return false;
        if (FAILED(dev->SetDataFormat(&joyFormat)))
        {
            dev->Release();
            return false;
        }
        DIDEVCAPS caps;
        caps.dwSize = sizeof(caps);
        dev->GetCapabilities(&caps);
        bool ff = (caps.dwFlags & DIDC_FORCEFEEDBACK) != 0 && window;
        // Retour de force : accès exclusif (au premier plan) ; sinon accès partagé, même fenêtre en arrière-plan
        if (!window || FAILED(dev->SetCooperativeLevel(window, ff ? (DISCL_EXCLUSIVE | DISCL_FOREGROUND) : (DISCL_NONEXCLUSIVE | DISCL_BACKGROUND))))
            ff = false;
        // Tous les axes ramenés à [-10000, 10000]
        DIPROPRANGE range;
        range.diph.dwSize = sizeof(DIPROPRANGE);
        range.diph.dwHeaderSize = sizeof(DIPROPHEADER);
        range.diph.dwHow = DIPH_DEVICE;
        range.diph.dwObj = 0;
        range.lMin = -10000;
        range.lMax = 10000;
        dev->SetProperty(DIPROP_RANGE, &range.diph);
        d.di = dev;
        d.instance = inst.guidInstance;
        d.info.name = inst.tszProductName;
        d.info.xinput = false;
        d.info.axes = (int)std::min<DWORD>(caps.dwAxes, Gamepad::MAX_AXES);
        d.info.buttons = (int)std::min<DWORD>(caps.dwButtons, Gamepad::MAX_BUTTONS);
        if (ff)
        {
            // Ressort de rappel du pilote coupé : c'est le jeu qui donne la force
            DIPROPDWORD autoCenter;
            autoCenter.diph.dwSize = sizeof(DIPROPDWORD);
            autoCenter.diph.dwHeaderSize = sizeof(DIPROPHEADER);
            autoCenter.diph.dwHow = DIPH_DEVICE;
            autoCenter.diph.dwObj = 0;
            autoCenter.dwData = DIPROPAUTOCENTER_OFF;
            dev->SetProperty(DIPROP_AUTOCENTER, &autoCenter.diph);
            dev->Acquire();
            DWORD axes[1] = {0}; // DIJOFS_X : le volant
            LONG direction[1] = {0};
            DICONSTANTFORCE cf = {0};
            DIEFFECT eff;
            std::memset(&eff, 0, sizeof(eff));
            eff.dwSize = sizeof(DIEFFECT);
            eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
            eff.dwDuration = INFINITE;
            eff.dwGain = DI_FFNOMINALMAX;
            eff.dwTriggerButton = DIEB_NOTRIGGER;
            eff.cAxes = 1;
            eff.rgdwAxes = axes;
            eff.rglDirection = direction;
            eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
            eff.lpvTypeSpecificParams = &cf;
            if (SUCCEEDED(dev->CreateEffect(kConstantForce, &eff, &d.effect, nullptr)) && d.effect)
                d.effect->Start(1, 0);
        }
        d.info.forceFeedback = d.effect != nullptr;
        dev->Acquire();
        return true;
    }

    void pollXInput(Device &d)
    {
        XINPUT_STATE xs;
        std::memset(&xs, 0, sizeof(xs));
        Gamepad::State &s = d.state;
        if (!xGetState || xGetState((DWORD)d.xinputSlot, &xs) != ERROR_SUCCESS)
        {
            s = Gamepad::State{};
            xConnected[d.xinputSlot] = false;
            return;
        }
        const XINPUT_GAMEPAD &g = xs.Gamepad;
        s.connected = true;
        s.axis[Gamepad::X_LEFT_X] = std::max(-1.0f, g.sThumbLX / 32767.0f);
        s.axis[Gamepad::X_LEFT_Y] = std::max(-1.0f, g.sThumbLY / 32767.0f);
        s.axis[Gamepad::X_RIGHT_X] = std::max(-1.0f, g.sThumbRX / 32767.0f);
        s.axis[Gamepad::X_RIGHT_Y] = std::max(-1.0f, g.sThumbRY / 32767.0f);
        s.axis[Gamepad::X_LEFT_TRIGGER] = g.bLeftTrigger / 255.0f;
        s.axis[Gamepad::X_RIGHT_TRIGGER] = g.bRightTrigger / 255.0f;
        static const WORD bits[14] = {XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
                                      XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_BACK,
                                      XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
                                      XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT,
                                      XINPUT_GAMEPAD_DPAD_RIGHT};
        for (int i = 0; i < 14; ++i)
            s.button[i] = (g.wButtons & bits[i]) != 0;
        int dx = (s.button[Gamepad::X_DPAD_RIGHT] ? 1 : 0) - (s.button[Gamepad::X_DPAD_LEFT] ? 1 : 0);
        int dy = (s.button[Gamepad::X_DPAD_UP] ? 1 : 0) - (s.button[Gamepad::X_DPAD_DOWN] ? 1 : 0);
        s.pov = (dx || dy) ? ((int)std::lround(std::atan2((float)dx, (float)dy) * 57.29578f) + 360) % 360 : -1;
    }

    void pollDirectInput(Device &d)
    {
        Gamepad::State &s = d.state;
        if (FAILED(d.di->Poll()))
        {
            d.di->Acquire();
            if (FAILED(d.di->Poll()))
            {
                // Fenêtre en arrière-plan (accès exclusif perdu) : on garde le dernier état, commandes relâchées
                for (bool &b : s.button)
                    b = false;
                return;
            }
            d.lastForce = 1 << 30; // effet à renvoyer après la reprise
        }
        DIJOYSTATE js;
        if (FAILED(d.di->GetDeviceState(sizeof(DIJOYSTATE), &js)))
            return;
        s.connected = true;
        const LONG values[8] = {js.lX, js.lY, js.lZ, js.lRx, js.lRy, js.lRz, js.rglSlider[0], js.rglSlider[1]};
        for (int i = 0; i < 8; ++i)
            s.axis[i] = std::max(-1.0f, std::min(1.0f, values[i] / 10000.0f));
        for (int i = 0; i < 32; ++i)
            s.button[i] = (js.rgbButtons[i] & 0x80) != 0;
        s.pov = LOWORD(js.rgdwPOV[0]) == 0xFFFF ? -1 : (int)(js.rgdwPOV[0] / 100);
    }
}

namespace Gamepad
{
    void init(void *windowHandle)
    {
        window = windowHandle ? (HWND)windowHandle : WindowFromDC(wglGetCurrentDC());
        buildFormat();
        for (const char *dll : {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"})
            if ((xinputDll = LoadLibraryA(dll)) != nullptr)
                break;
        if (xinputDll)
        {
            xGetState = (XInputGetStateFn)(void *)GetProcAddress(xinputDll, "XInputGetState");
            xSetState = (XInputSetStateFn)(void *)GetProcAddress(xinputDll, "XInputSetState");
        }
        dinputDll = LoadLibraryA("dinput8.dll");
        if (dinputDll)
        {
            auto create = (DirectInput8CreateFn)(void *)GetProcAddress(dinputDll, "DirectInput8Create");
            if (!create || FAILED(create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, kIID_IDirectInput8A, (LPVOID *)&directInput, nullptr)))
                directInput = nullptr;
        }
        refresh();
    }

    void shutdown()
    {
        for (auto &d : devices)
        {
            if (d.xinputSlot >= 0)
                rumble((int)(&d - devices.data()), 0.0f, 0.0f);
            releaseDevice(d);
        }
        devices.clear();
        if (directInput)
            directInput->Release();
        directInput = nullptr;
    }

    void refresh()
    {
        std::vector<Device> next;
        // Manettes XInput branchées
        for (int slot = 0; slot < 4; ++slot)
        {
            XINPUT_STATE xs;
            xConnected[slot] = xGetState && xGetState((DWORD)slot, &xs) == ERROR_SUCCESS;
            if (!xConnected[slot])
                continue;
            Device d;
            d.xinputSlot = slot;
            d.info.name = "Manette Xbox " + std::to_string(slot + 1);
            d.info.xinput = true;
            d.info.axes = 6;
            d.info.buttons = 14;
            next.push_back(d);
        }
        // Autres contrôleurs (volants, pédaliers, manettes génériques) ; ceux déjà ouverts sont gardés
        if (directInput)
        {
            EnumContext ctx;
            directInput->EnumDevices(DI8DEVCLASS_GAMECTRL, enumDevice, &ctx, DIEDFL_ATTACHEDONLY);
            std::vector<DWORD> xinput = xinputProducts();
            for (const auto &inst : ctx.found)
            {
                if (std::find(xinput.begin(), xinput.end(), inst.guidProduct.Data1) != xinput.end())
                    continue;
                auto old = std::find_if(devices.begin(), devices.end(), [&](const Device &o)
                                        { return o.di && IsEqualGUID(o.instance, inst.guidInstance); });
                if (old != devices.end())
                {
                    next.push_back(*old);
                    old->di = nullptr; // transféré
                    old->effect = nullptr;
                    continue;
                }
                Device d;
                if (openDirectInput(d, inst))
                    next.push_back(d);
            }
        }
        for (auto &d : devices)
            releaseDevice(d); // débranchés
        devices.swap(next);
        lastXInputScan = GetTickCount();
    }

    void update()
    {
        // Une manette XInput branchée en cours de partie apparaît d'elle-même (contrôle toutes les 2 s)
        if (GetTickCount() - lastXInputScan > 2000)
        {
            lastXInputScan = GetTickCount();
            for (int slot = 0; slot < 4; ++slot)
            {
                XINPUT_STATE xs;
                bool now = xGetState && xGetState((DWORD)slot, &xs) == ERROR_SUCCESS;
                if (now != xConnected[slot])
                {
                    refresh();
                    break;
                }
            }
        }
        for (auto &d : devices)
        {
            if (d.xinputSlot >= 0)
                pollXInput(d);
            else if (d.di)
                pollDirectInput(d);
        }
    }

    int count() { return (int)devices.size(); }

    const DeviceInfo &info(int device)
    {
        static DeviceInfo none;
        return device >= 0 && device < count() ? devices[device].info : none;
    }

    const State &state(int device)
    {
        static State none;
        return device >= 0 && device < count() ? devices[device].state : none;
    }

    void rumble(int device, float low, float high)
    {
        if (device < 0 || device >= count() || devices[device].xinputSlot < 0 || !xSetState)
            return;
        Device &d = devices[device];
        low = std::max(0.0f, std::min(1.0f, low));
        high = std::max(0.0f, std::min(1.0f, high));
        if (std::fabs(low - d.rumbleLow) < 0.02f && std::fabs(high - d.rumbleHigh) < 0.02f)
            return;
        d.rumbleLow = low;
        d.rumbleHigh = high;
        XINPUT_VIBRATION v;
        v.wLeftMotorSpeed = (WORD)(low * 65535.0f);
        v.wRightMotorSpeed = (WORD)(high * 65535.0f);
        xSetState((DWORD)d.xinputSlot, &v);
    }

    void setForce(int device, float force)
    {
        if (device < 0 || device >= count() || !devices[device].effect)
            return;
        Device &d = devices[device];
        LONG magnitude = (LONG)(std::max(-1.0f, std::min(1.0f, force)) * 10000.0f);
        if (std::abs(magnitude - d.lastForce) < 50)
            return;
        d.lastForce = magnitude;
        DWORD axes[1] = {0};
        LONG direction[1] = {0};
        DICONSTANTFORCE cf;
        cf.lMagnitude = magnitude;
        DIEFFECT eff;
        std::memset(&eff, 0, sizeof(eff));
        eff.dwSize = sizeof(DIEFFECT);
        eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
        eff.cAxes = 1;
        eff.rgdwAxes = axes;
        eff.rglDirection = direction;
        eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
        eff.lpvTypeSpecificParams = &cf;
        d.effect->SetParameters(&eff, DIEP_TYPESPECIFICPARAMS | DIEP_START);
    }

    const char *axisName(int device, int axis)
    {
        static const char *xNames[6] = {"Stick gauche X", "Stick gauche Y", "Stick droit X", "Stick droit Y", "Gachette LT", "Gachette RT"};
        static const char *dNames[8] = {"Axe X", "Axe Y", "Axe Z", "Axe Rx", "Axe Ry", "Axe Rz", "Curseur 1", "Curseur 2"};
        if (axis < 0 || axis >= MAX_AXES)
            return "Aucun";
        if (info(device).xinput)
            return axis < 6 ? xNames[axis] : "Aucun";
        return dNames[axis];
    }

    std::string buttonName(int device, int button)
    {
        static const char *xNames[14] = {"A", "B", "X", "Y", "LB", "RB", "BACK", "START", "L3", "R3", "Croix haut", "Croix bas", "Croix gauche", "Croix droite"};
        if (button < 0)
            return "Aucun";
        if (info(device).xinput)
            return button < 14 ? xNames[button] : "Aucun";
        return "Bouton " + std::to_string(button + 1);
    }
}

#else

namespace Gamepad
{
    void init(void *) {}
    void shutdown() {}
    void refresh() {}
    void update() {}
    int count() { return 0; }
    const DeviceInfo &info(int)
    {
        static DeviceInfo none;
        return none;
    }
    const State &state(int)
    {
        static State none;
        return none;
    }
    void rumble(int, float, float) {}
    void setForce(int, float) {}
    const char *axisName(int, int) { return "Aucun"; }
    std::string buttonName(int, int) { return "Aucun"; }
}

#endif
