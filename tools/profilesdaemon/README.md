##profilesdaemon

###Tool description

Profilesdaemon is a tool for profiles management and configuration distribution

###Management

Tool provides API for profiles.xml modification (adding/editing/removing profiles and channels).
It uses UNIX datagram socket for accepting commands and sending responses in JSON format.

###Distribution

Profilesdaemon can distribute profiles configuration to multiple IPFIX collectors.
Tool accepts new collectors on TCP network socket and keeps connection opened.
Configuration is sent to all active collectors after performing some changes.
IPFIX collector must be running with profiler intermediate plugin.

###API

Here is an acceptable JSON message format with all possible requests:

```json
{
	"requests":
	[
		{
			"type": "removeProfile",
			"path": "path/to/profile"
		},
		{
			"type": "removeChannel",
			"path": "live/subprofile/channel"
		},
		{
			"type": "addProfile",
			"path": "path/to/new/profile"
		},
		{
			"type": "addChannel",
			"path": "path/to/new/channel",
			"sources": ["list", "of", "sources"],
			"filter": "filter string"
		},
		{
			"type": "editProfile",
			"path": "path/to/existing/profile",
			"name": "newNameInSameLocation"
		},
		{
			"type": "editChannel",
			"path": "path/to/existing/channel",
			// OPTIONAL:
			"name": "newNameInSameLocation",
			"sources": ["list", "of", "new", "sources"],
			"filter": "newFilter"
		}
	],
	"save": true,
}

```

Response:

```json
{
	"status": "OK",
	//OPTIONAL
	"messages": [
		"Some message",
	]
}

```
or:

```json

{
	"status": "Error",
	"messages": [
		"Cannot create new profile...",
		"Cannot save data",
	]
}

```

[Back to Top](#top)
