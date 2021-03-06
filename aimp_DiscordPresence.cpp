#include "AIMPRemote.h"
#include "AIMPPlugin.h"
#include "DiscordRPC.h"
#include <time.h>

CONST CHAR *AppId			= "429559336982020107";
CONST WCHAR *InfoName		= L"Discord Presence";
CONST WCHAR *InfoAuthor		= L"Exle";
CONST WCHAR *InfoShortDesc	= L"Update your discord status with the rich presence";

AIMPRemote *aimpRemote;
DiscordRPC *discord;
DiscordRichPresence discord_rpc;

AIMPPlugin::AIMPPlugin()
	: bFinalised(true)
{
	AddRef();
}

AIMPPlugin::~AIMPPlugin()
{
	Finalize();
}

HRESULT __declspec(dllexport) WINAPI AIMPPluginGetHeader(IAIMPPlugin **Header)
{
	*Header = new AIMPPlugin();
	return S_OK;
}

PWCHAR WINAPI AIMPPlugin::InfoGet(INT Index)
{
	switch (Index)
	{
		case AIMP_PLUGIN_INFO_NAME:					return const_cast<PWCHAR>(InfoName);
		case AIMP_PLUGIN_INFO_AUTHOR:				return const_cast<PWCHAR>(InfoAuthor);
		case AIMP_PLUGIN_INFO_SHORT_DESCRIPTION:	return const_cast<PWCHAR>(InfoShortDesc);
	}

	return nullptr;
}

DWORD WINAPI AIMPPlugin::InfoGetCategories()
{
	return AIMP_PLUGIN_CATEGORY_ADDONS;
}

HRESULT WINAPI AIMPPlugin::Initialize(IAIMPCore* Core)
{
	if (!bFinalised)
	{
		return E_ABORT;
	}

	HWND AIMPRemoteHandle = FindWindowA(AIMPRemoteAccessClass, AIMPRemoteAccessClass);
	if (AIMPRemoteHandle == NULL)
	{
		return E_ABORT;
	}

	aimpRemote	= new AIMPRemote();
	discord		= new DiscordRPC();

	DiscordEventHandlers handlers = { 0 };
	handlers.ready = DiscordReady;

	discord->Initialise(AppId, &handlers);

	discord_rpc.largeImageKey = "aimp";
	discord_rpc.smallImageKey = "aimp";
	discord_rpc.smallImageText = "AIMP";
	discord_rpc.instance = false;

	discord->UpdateRP(&discord_rpc);

	AIMPEvents Events = { 0 };
	Events.State			= UpdatePlayerState;
	Events.TrackInfo		= UpdateTrackInfo;
	Events.TrackPosition	= UpdateTrackPosition;

	aimpRemote->AIMPSetEvents(&Events);
	aimpRemote->AIMPSetRemoteHandle(&AIMPRemoteHandle);

	bFinalised = false;
	return S_OK;
}

HRESULT WINAPI AIMPPlugin::Finalize()
{
	if (discord)
	{
		delete discord;
	}

	if (aimpRemote)
	{
		delete aimpRemote;
	}

	return S_OK;
}

VOID WINAPI AIMPPlugin::SystemNotification(INT NotifyID, IUnknown* Data) {}

VOID AIMPPlugin::DiscordReady(VOID)
{
	UpdateTrackInfo(&aimpRemote->AIMPGetTrackInfo());
}

VOID AIMPPlugin::UpdatePlayerState(INT AIMPRemote_State)
{
	if (AIMPRemote_State == AIMPREMOTE_PLAYER_STATE_PLAYING)
	{
		UpdateTrackInfo(&aimpRemote->AIMPGetTrackInfo());
		//discord->FastUpdate();
	}
	else
	{
		discord->ClearPresence();
	}
}

VOID AIMPPlugin::UpdateTrackPosition(PAIMPPosition AIMPRemote_Position)
{
	discord->RunCallbacks();
}

VOID AIMPPlugin::UpdateTrackInfo(PAIMPTrackInfo AIMPRemote_TrackInfo)
{
	discord_rpc.state			= AIMPRemote_TrackInfo->Artist;
	discord_rpc.details			= AIMPRemote_TrackInfo->Title;
	discord_rpc.largeImageText	= AIMPRemote_TrackInfo->Album;

	discord_rpc.startTimestamp	= 0;
	discord_rpc.endTimestamp	= 0;

	int Duration = aimpRemote->AIMPGetPropertyValue(AIMP_RA_PROPERTY_PLAYER_DURATION) / 1000;
	int Position = aimpRemote->AIMPGetPropertyValue(AIMP_RA_PROPERTY_PLAYER_POSITION) / 1000;
	if (Duration != 0)
	{
		discord_rpc.startTimestamp = time(0);
		discord_rpc.endTimestamp = discord_rpc.startTimestamp + Duration - Position;
	}

	if (strncmp(AIMPRemote_TrackInfo->FileName, "http://", 7) == 0 || strncmp(AIMPRemote_TrackInfo->FileName, "https://", 8) == 0)
	{
		if (!AIMPRemote_TrackInfo->Album[0])
		{
			discord_rpc.largeImageText = "Listening to URL";
		}

		discord_rpc.largeImageKey = "aimp_radio";
	}
	else discord_rpc.largeImageKey = "aimp";

	discord->UpdateRP(&discord_rpc);

	if (aimpRemote->AIMPGetPropertyValue(AIMP_RA_PROPERTY_PLAYER_STATE) == AIMPREMOTE_PLAYER_STATE_PLAYING)
	{
		discord->FastUpdate();
	}
}