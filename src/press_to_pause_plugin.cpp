#include "log.h"
#include "menu_framework_api.h"

#include <Windows.h>
#include <string>

namespace
{
	constexpr std::uint32_t kDefaultKeyboardKey = 0x3B;
	constexpr std::uint32_t kDefaultGamepadKey = 275;

	class PressToPauseController final : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		enum class BindingTarget
		{
			kNone,
			kKeyboard,
			kGamepad
		};

		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
			RE::BSTEventSource<RE::InputEvent*>*) override
		{
			if (!a_event || !*a_event) {
				return RE::BSEventNotifyControl::kContinue;
			}

			if (bindingTarget != BindingTarget::kNone) {
				return RE::BSEventNotifyControl::kContinue;
			}

			for (auto event = *a_event; event; event = event->next) {
				const auto button = event->AsButtonEvent();
				if (!button || !button->IsDown()) {
					continue;
				}

				const auto device = button->GetDevice();
				const auto key = button->GetIDCode();
				if ((device == RE::INPUT_DEVICE::kKeyboard && key == keyboardKey) ||
					(device == RE::INPUT_DEVICE::kGamepad && key == gamepadKey)) {
					TogglePause();
					break;
				}
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		void LoadSettings()
		{
			auto path = SKSE::log::log_directory();
			if (!path) {
				return;
			}
			*path /= "PressToPause.ini";
			const auto iniPath = path->string();
			keyboardKey = GetPrivateProfileIntA("Controls", "KeyboardKey", kDefaultKeyboardKey, iniPath.c_str());
			gamepadKey = GetPrivateProfileIntA("Controls", "GamepadKey", kDefaultGamepadKey, iniPath.c_str());
		}

		void RenderMenu()
		{
			auto& menu = MenuFramework::Get();
			menu.Text("Keyboard pause/continue key");
			menu.SameLine();
			if (menu.Button(bindingTarget == BindingTarget::kKeyboard ? "Press keyboard key..." : "Bind keyboard key")) {
				bindingTarget = BindingTarget::kKeyboard;
			}
			menu.Text(("Keyboard key code: " + std::to_string(keyboardKey)).c_str());

			menu.Text("Gamepad pause/continue key");
			menu.SameLine();
			if (menu.Button(bindingTarget == BindingTarget::kGamepad ? "Press gamepad button..." : "Bind gamepad button")) {
				bindingTarget = BindingTarget::kGamepad;
			}
			menu.Text(("Gamepad key code: " + std::to_string(gamepadKey)).c_str());

			if (bindingTarget != BindingTarget::kNone) {
				menu.Text("Press the selected key or button. Escape cancels keyboard binding.");
				if (menu.Button("Cancel binding")) {
					bindingTarget = BindingTarget::kNone;
				}
			}
		}

		bool CaptureBinding(RE::InputEvent* a_event)
		{
			if (bindingTarget == BindingTarget::kNone || !a_event) {
				return false;
			}

			const auto button = a_event->AsButtonEvent();
			if (!button || !button->IsDown()) {
				return false;
			}

			const auto device = button->GetDevice();
			const auto key = button->GetIDCode();
			if (bindingTarget == BindingTarget::kKeyboard && device == RE::INPUT_DEVICE::kKeyboard) {
				if (key == 1) {
					bindingTarget = BindingTarget::kNone;
					return true;
				}
				keyboardKey = key;
				SaveSetting("Controls", "KeyboardKey", keyboardKey);
			} else if (bindingTarget == BindingTarget::kGamepad && device == RE::INPUT_DEVICE::kGamepad) {
				gamepadKey = key;
				SaveSetting("Controls", "GamepadKey", gamepadKey);
			} else {
				return false;
			}

			bindingTarget = BindingTarget::kNone;
			return true;
		}

		void RegisterInput()
		{
			if (const auto input = RE::BSInputDeviceManager::GetSingleton()) {
				input->AddEventSink(this);
			}
		}

		void ResetForLoad()
		{
			if (pausedByPlugin) {
				SetPaused(false);
			}
		}

	private:
		void SaveSetting(const char* a_section, const char* a_key, std::uint32_t a_value)
		{
			if (auto path = SKSE::log::log_directory()) {
				*path /= "PressToPause.ini";
				const auto iniPath = path->string();
				const auto value = std::to_string(a_value);
				WritePrivateProfileStringA(a_section, a_key, value.c_str(), iniPath.c_str());
			}
		}

		void TogglePause()
		{
			SetPaused(!pausedByPlugin);
		}

		void SetPaused(bool a_paused)
		{
			if (pausedByPlugin == a_paused) {
				return;
			}

			const auto ui = RE::UI::GetSingleton();
			if (!ui) {
				return;
			}

			if (a_paused) {
				++ui->numPausesGame;
			} else if (ui->numPausesGame > 0) {
				--ui->numPausesGame;
			}
			pausedByPlugin = a_paused;
		}

		std::uint32_t keyboardKey{ kDefaultKeyboardKey };
		std::uint32_t gamepadKey{ kDefaultGamepadKey };
		bool pausedByPlugin{ false };
		BindingTarget bindingTarget{ BindingTarget::kNone };
	};

	PressToPauseController g_pressToPauseController;
	std::int64_t g_inputRegistration{ 0 };

	void __stdcall RenderMenu()
	{
		g_pressToPauseController.RenderMenu();
	}

	bool __stdcall CaptureMenuInput(RE::InputEvent* a_event)
	{
		return g_pressToPauseController.CaptureBinding(a_event);
	}

	void RegisterMenu()
	{
		auto& framework = MenuFramework::Get();
		if (!framework.Initialize()) {
			spdlog::warn("SKSE Menu Framework API unavailable; Press to Pause settings menu disabled");
			return;
		}

		framework.AddSectionItem("Press to Pause/Controls", RenderMenu);
		g_inputRegistration = framework.RegisterInput(CaptureMenuInput);
	}

	void MessageHandler(SKSE::MessagingInterface::Message* a_message)
	{
		if (!a_message) {
			return;
		}

		switch (a_message->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			g_pressToPauseController.LoadSettings();
			g_pressToPauseController.RegisterInput();
			break;
		case SKSE::MessagingInterface::kPostPostLoad:
			RegisterMenu();
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			g_pressToPauseController.ResetForLoad();
			break;
		default:
			break;
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	SetupLog();

	const auto messaging = SKSE::GetMessagingInterface();
	return messaging && messaging->RegisterListener("SKSE", MessageHandler);
}
