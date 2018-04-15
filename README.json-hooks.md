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

For protocol version `0.1`, each JSON object must be encoded on a single
line.

## Lifecycle

The general life of a hook is as following.

1. Hook is started
2. Hello handshake is exchanged
3. One or more calls or notifications are sent from apt to the hook
4. Bye notification is send

It is unspecified whether a hook is sent one or more messages. For
example, a hook may be started only once for the lifetime of the apt
process and receive multiple notificatgions, but a hook may also be
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

1. `org.debian.apt.hooks.install.pre-prompt` - Run before the y/n prompt
1. `org.debian.apt.hooks.install.post` - Run after success
1. `org.debian.apt.hooks.install.fail` - Run after failed instal
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

- *mode*: One of `install`, `deinstall`, `purge`, or `keep`. `keep`
          is not exposed in 0.1. To determine an upgrade, check
          that a current version is installed.
- *automatic*: Whether the package is/will be automatically installed
- *versions*: An array with up to 3 fields:

  - *candidate*: The candidate version
  - *install*: The version to be installed
  - *current*: The version currently installed

  Each version is represented as an object with the following fields:

  - *id*: An unsigned integer
  - *version*: The version as a string
  - *architecture*: Architecture of the version
  - *pin*: The pin priority

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
