#include <Utilities.h>
using namespace RE;
using std::unordered_map;

REL::Relocation<uintptr_t> ptr_SprintCheck{ REL::ID(218087), 0x4C };

PlayerCharacter* pc;
PlayerCamera* pcam;
PlayerControls* pcon;

class TransformDeltaEventWatcher
{
public:
	typedef BSEventNotifyControl (TransformDeltaEventWatcher::*FnProcessEvent)(BSTransformDeltaEvent& evn, BSTEventSource<BSTransformDeltaEvent>* dispatcher);

	BSEventNotifyControl HookedProcessEvent(BSTransformDeltaEvent& evn, BSTEventSource<BSTransformDeltaEvent>* src)
	{
		ActorEx* a = (ActorEx*)((uintptr_t)this - 0x140);
		if (a->GetDesiredSpeed() <= 0) {
			evn.currentTranslation = evn.previousTranslation;
			evn.deltaTranslation.pt[0] /= 5.f;
			evn.deltaTranslation.pt[1] /= 5.f;
			evn.deltaTranslation.pt[2] /= 5.f;
		}
		FnProcessEvent fn = fnHash.at(*(uint64_t*)this);
		return fn ? (this->*fn)(evn, src) : BSEventNotifyControl::kContinue;
	}

	void HookSink()
	{
		uint64_t vtable = *(uint64_t*)this;
		auto it = fnHash.find(vtable);
		if (it == fnHash.end()) {
			FnProcessEvent fn = SafeWrite64Function(vtable + 0x8, &TransformDeltaEventWatcher::HookedProcessEvent);
			fnHash.insert(std::pair<uint64_t, FnProcessEvent>(vtable, fn));
		}
	}

protected:
	static unordered_map<uintptr_t, FnProcessEvent> fnHash;
};
unordered_map<uintptr_t, TransformDeltaEventWatcher::FnProcessEvent> TransformDeltaEventWatcher::fnHash;

void InitializePlugin()
{
	pc = PlayerCharacter::GetSingleton();
	((TransformDeltaEventWatcher*)((uintptr_t)pc + 0x140))->HookSink();
	pcam = PlayerCamera::GetSingleton();
	pcon = PlayerControls::GetSingleton();
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
#ifndef NDEBUG
	auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
	auto path = logger::log_directory();
	if (!path) {
		return false;
	}

	*path /= fmt::format(FMT_STRING("{}.log"), Version::PROJECT);
	auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

	auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));

#ifndef NDEBUG
	log->set_level(spdlog::level::trace);
#else
	log->set_level(spdlog::level::info);
	log->flush_on(spdlog::level::warn);
#endif

	spdlog::set_default_logger(std::move(log));
	spdlog::set_pattern("%g(%#): [%^%l%$] %v"s);

	logger::info(FMT_STRING("{} v{}"), Version::PROJECT, Version::NAME);

	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::PROJECT.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		logger::critical("loaded in editor"sv);
		return false;
	}

	const auto ver = a_f4se->RuntimeVersion();
	if (ver < F4SE::RUNTIME_1_10_162) {
		logger::critical(FMT_STRING("unsupported runtime v{}"), ver.string());
		return false;
	}

	return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);

	uint8_t bytes[] = { 0xEB };
	REL::safe_write<uint8_t>(ptr_SprintCheck.address(), std::span{ bytes });

	const F4SE::MessagingInterface* message = F4SE::GetMessagingInterface();
	message->RegisterListener([](F4SE::MessagingInterface::Message* msg) -> void {
		if (msg->type == F4SE::MessagingInterface::kGameDataReady) {
			InitializePlugin();
		}
	});

	return true;
}
