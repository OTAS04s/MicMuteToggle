// MicMuteToggle.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "windows.h"
#include "mmdeviceapi.h"
#include "mmsystem.h"
#include "endpointvolume.h"
#include "MicMuteToggle.h"

// Global Variables:
HINSTANCE hInst;                                // current instance
HANDLE hSingleInstanceMutex;
HHOOK hMouseHook;
HHOOK hKeyboardHook;

// Forward declarations of functions included in this code module:
BOOL                InitInstance(HINSTANCE, int);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: Place code here.

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MICMUTETOGGLE));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	ReleaseMutex(hSingleInstanceMutex);

	return (int)msg.wParam;
}

enum class MuteBehavior {
	TOGGLE = 0,
	MUTE = 1,
	UNMUTE = 2,
};

void SetMute(MuteBehavior SetTo, bool IsButtonUp) {
	IMMDeviceEnumerator* de;
	CoCreateInstance(
		__uuidof(MMDeviceEnumerator), NULL,
		CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
		(void**)&de
	);

	IMMDevice* micDevicePtr;
	de->GetDefaultAudioEndpoint(EDataFlow::eCapture, ERole::eCommunications, &micDevicePtr);

	IAudioEndpointVolume* micVolume;
	micDevicePtr->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, (void**)&micVolume);
	BOOL wasMuted;
	micVolume->GetMute(&wasMuted);
	if (wasMuted && SetTo == MuteBehavior::MUTE) {
		return;
	}
	if (!wasMuted && SetTo == MuteBehavior::UNMUTE) {
		return;
	}

	const auto muteWav = MAKEINTRESOURCE(IDR_MUTE);
	const auto unmuteWav = MAKEINTRESOURCE(IDR_UNMUTE);
	const auto feedbackWav = wasMuted ? unmuteWav : muteWav;

	if (!wasMuted) {
		if (IsButtonUp) {
			// Push to talk released. Everyone releases the button once they've started the last syllable,
			// but that's not understandable if you actually cut the microphone there. Wait a little longer.
			Sleep(250 /* ms */);
		}
		micVolume->SetMute(TRUE, nullptr);
	}
	else {
		micVolume->SetMute(FALSE, nullptr);
	}
	PlaySound(feedbackWav, hInst, SND_ASYNC | SND_RESOURCE);
}

LRESULT CALLBACK GlobalMouseHook(
	int code,
	WPARAM wParam,
	LPARAM lParam
) {
	if (code != 0 || lParam == 0 || (wParam != WM_XBUTTONUP && wParam != WM_XBUTTONDOWN)) {
		return CallNextHookEx(0, code, wParam, lParam);
	}
	//UnhookWindowsHookEx(hMouseHook);
	//UnhookWindowsHookEx(hKeyboardHook);

	MSLLHOOKSTRUCT* event = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
	if ((event->mouseData >> 16) != XBUTTON2) {
		return CallNextHookEx(0, code, wParam, lParam);
	}

	SetMute(MuteBehavior::TOGGLE, wParam == WM_XBUTTONUP);

	return S_OK;
}

LRESULT CALLBACK GlobalKeyboardHook(
	int code,
	WPARAM wParam,
	LPARAM lParam
) {
	if (code != 0 || lParam == 0 || (wParam != WM_KEYDOWN && wParam != WM_KEYUP)) {
		return CallNextHookEx(0, code, wParam, lParam);
	}

	KBDLLHOOKSTRUCT* event = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
	if (event->vkCode == VK_F15) {
		if (wParam == WM_KEYDOWN) {
			SetMute(MuteBehavior::MUTE, /* button up = */ false);
		}
		return S_OK;
	}
	if (event->vkCode == VK_F16) {
		if (wParam == WM_KEYDOWN) {
			SetMute(MuteBehavior::UNMUTE, /* button up = */ false);
		}
		return S_OK;
	}

	return CallNextHookEx(0, code, wParam, lParam);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance;

	CoInitialize(NULL); // Initalize COM

	hSingleInstanceMutex = CreateMutex(nullptr, FALSE, L"Global\\com.fredemmott.micmutetoggle");
	if (GetLastError() == ERROR_ALREADY_EXISTS) {
		return FALSE;
	}

	hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, GlobalMouseHook, nullptr, 0); // Hook low-level mouse events, e.g. button presses
	hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, GlobalKeyboardHook, nullptr, 0); // Same for keyboard
	return TRUE;
}
