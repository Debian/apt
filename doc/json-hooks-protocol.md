Version: 0.2

## JSON Hooks

APT 1.6 introduces support for hooks that talk JSON-RPC 2.0. Hooks act
as a server, and APT as a client.

## Wire protocol

APT communicates with hooks via a UNIX domain socket in file descriptor
`$APT_HOOK_SOCKET`. The transport is a byte stream (SOCK_STREAM).

The byte stream contains multiple JSON objects, each representing a
JSON-RPC request or response, and each terminated by an empty line
(`\n\n`). Therefore, JSON objects containing empty lines may not be
used.

Each JSON object must be encoded on a single line at the moment,
but this may change in future versions.

## Lifecycle

The general life of a hook is as following.

1. Hook is started
2. Hello handshake is exchanged
3. One or more calls or notifications are sent from apt to the hook
4. Bye notification is sent

It is unspecified whether a hook is sent one or more messages. For
example, a hook may be started only once for the lifetime of the apt
process and receive multiple notifications, but a hook may also be
started multiple times. Hooks should thus be stateless.

## JSON messages

### Hello handshake

APT performs a call to the method `org.debian.apt.hooks.hello` with
the named parameter `versions` containing a list of supported protocol
versions. The hook picks the version it supports. The current version
is `"0.1"`, and support for that version is mandatory.

*Example*:

1. APT:
   ```{"jsonrpc":"2.0","method":"org.debian.apt.hooks.hello","id":0,"params":{"versions":["0.1"]}}```


2. Hook:
   ```{"jsonrpc":"2.0","id":0,"result":{"version":"0.1"}}```

### Bye notification

Before closing the connection, APT sends a notification for the
method `org.debian.apt.hooks.bye`.

### Hook notification

The following methods are supported:

1. `org.debian.apt.hooks.install.pre-prompt` - Run before the package list and y/n prompt
1. `org.debian.apt.hooks.install.package-list` - (optional in 0.1) Run after the package list. You could display additional lists of packages here
1. `org.debian.apt.hooks.install.statistics` - (optional in 0.1) Run after the count of packages to upgrade/install. You could display additional information here, such as `5 security upgrades`
1. `org.debian.apt.hooks.install.post` - Run after success
1. `org.debian.apt.hooks.install.fail` - Run after failed install
1. `org.debian.apt.hooks.search.pre` - Run before search
1. `org.debian.apt.hooks.search.post` - Run after successful search
1. `org.debian.apt.hooks.search.fail` - Run after search without results

They can be registered by adding them to the list:

```AptCli::Hooks::<name>```

where `<name>` is the name of the hook. It is recommended that these
option names are prefixed with `Binary::apt`, so that they only take
effect for the `apt` binary. Otherwise, there may be compatibility issues
with scripts and alike.

#### Parameters

*command*: The command used on the command-line. For example, `"purge"`.

*search-terms*: Any non-option arguments given to the command.

*unknown-packages*: For non-search hooks, a subset of *search-terms*
that APT could not find packages in the cache for.

*packages*: An array of modified packages. This is mostly useful for
install. Each package has the following attributes:

- *id*: An unsigned integer describing the package
- *name*: The name of the package
- *architecture*: The architecture of the package. For `"all"` packages, this will be the native architecture;
			  use per-version architecture fields to see `"all"`.

- *mode*: One of `install`, `upgrade`, `downgrade`, `reinstall`, `deinstall`, `purge`, `keep`.
  Version 0.1 does not implement `upgrade`, `downgrade`, and `reinstall` - all of them are represented
  as `install`, and you have to compare the `current` version to the `install` version to figure out if
  is one of those.
- One of the following optional fields may be set to true to indicate a change relative to an installed version:
- *downgrade*: true if downgrading
- *upgrade*: true if upgrading
- *reinstall*: true if reinstall flag is set
- *automatic*: Whether the package is/will be automatically installed
- *versions*: An array with up to 3 fields:

- *candidate*: The candidate version
- *install*: The version to be installed
- *current*: The version currently installed

Each version is represented as an object with the following fields:

- *id*: An unsigned integer
- *version*: The version as a string
- *architecture*: Architecture of the version
- *pin*: The pin priority (optional)
- *origins*: Sources from which the package is retrieved (since 0.2)

  Each origin is represented as an object with the following fields:

  - *archive*: string (optional)
  - *codename*: string (optional)
  - *version*: string (optional)
  - *origin*: string (optional)
  - *label*: string (optional)
  - *site*: string, empty for local repositories or when using mirror+file:/ method (optional)

#### Example

```json
{
    "jsonrpc": "2.0",
    "method": "org.debian.apt.hooks.install.pre",
    "params": {
        "command": "purge",
        "search-terms": [
            "petname-",
            "lxd+"
        ],
        "packages": [
            {
                "id": 1500,
                "name": "ebtables",
                "architecture": "amd64",
                "mode": "install",
                "automatic": 1,
                "versions": {
                    "candidate": {
                        "id": 376,
                        "version": "2.0.10.4-3.5ubuntu2",
                        "architecture": "amd64",
                        "pin": 990
                    },
                    "install": {
                        "id": 376,
                        "version": "2.0.10.4-3.5ubuntu2",
                        "architecture": "amd64",
                        "pin": 990
                    }
                }
            }
        ]
    }
}
```

#### Compatibility note
Future versions of APT might make these calls instead of notifications.

## Evolution of this protocol
New incompatible versions may be introduced with each new feature
release of apt (1.7, 1.8, etc). No backward compatibility is promised
until protocol 1.0: New stable feature releases may support a newer
protocol version only (for example, 1.7 may only support 0.2).

Additional fields may be added to objects without bumping the protocol
version.

# Changes:

## Version 0.2

The 0.2 protocol makes one incompatible change, and adds several new compatible changes, all of which are optional in 0.1,
but mandatory in 0.2.

* (incompatible change) The `mode` flag of arguments gained `upgrade`, `downgrade`, `reinstall` modes. All of these are `install`
* (compatible change) The hooks `org.debian.apt.hooks.install.package-list` and `org.debian.apt.hooks.install.statistics` have been added
* (compatible change) Version objects gained a new `origins` array
