# Deployment

This guide covers installing, verifying, disabling and removing `mod-world-party-scaling` on an
AzerothCore WotLK server. Commands assume the core repository root is the working directory and the
module is cloned from <https://github.com/Phanty78/mod-world-party-scaling> into
`modules/mod-world-party-scaling`:

```bash
git clone https://github.com/Phanty78/mod-world-party-scaling.git \
  modules/mod-world-party-scaling
```

The module is a static script module. It is compiled into `worldserver`; adding or removing it always
requires a CMake reconfigure and a worldserver rebuild. It has no SQL, no database table and no
persistent state, and it does not edit `worldserver.conf` or `conf/dist/env.ac`.

## Configuration file

The runtime reads `env/dist/etc/modules/mod_world_party_scaling.conf`. The repository ships
`modules/mod-world-party-scaling/conf/mod_world_party_scaling.conf.dist`; CMake installs it to
`env/dist/etc/modules/mod_world_party_scaling.conf.dist`, and the native installer (with
`AC_ENABLE_CONF_COPY_ON_INSTALL=1`, the default) or the Docker entrypoint copies it to `.conf` on first
deployment.

Both paths are in the default configuration directory:

```text
env/dist/etc/modules/mod_world_party_scaling.conf
env/dist/etc/modules/mod_world_party_scaling.conf.dist
```

The `.conf` file is the live one. The module logs its effective values at startup and after a reload, so
the active values are visible in the log even when a missing `.conf` leaves the built-in defaults in
use.

## Docker deployment

1. Confirm that the module is present:

   ```bash
   ls modules/mod-world-party-scaling/src
   ```

2. Rebuild the worldserver image. CMake discovers the module and its config file during the build:

   ```bash
   docker compose build ac-worldserver
   ```

3. Start the stack:

   ```bash
   docker compose up -d
   ```

   `docker compose up -d --build` combines steps 2 and 3.

4. On first start the entrypoint copies the installed default config files into the bind-mounted
   configuration directory, then copies every `modules/*.conf.dist` to its `.conf` counterpart without
   overwriting an existing file. The live config is therefore visible on the host at:

   ```text
   env/dist/etc/modules/mod_world_party_scaling.conf
   ```

5. Edit that host file. Inside the container the same file is
   `/azerothcore/env/dist/etc/modules/mod_world_party_scaling.conf`.

6. Apply changes with a reload or a restart:

   ```bash
   docker compose restart ac-worldserver
   ```

   For a reload, attach to the console and run `reload config`, or use `.reload config` in game as a
   GM with the reload command permission. Detach from the console with `Ctrl-p`, `Ctrl-q`.

### Verify the Docker deployment

Read the worldserver log from the host:

```bash
docker compose logs ac-worldserver | grep -F "mod-world-party-scaling"
```

or read the mounted log file:

```bash
grep -F "mod-world-party-scaling" env/dist/logs/Server.log
```

A loaded, enabled module writes a line beginning with:

```text
mod-world-party-scaling: enabled (open world only);
```

followed by the six health tiers, six damage tiers and six XP tiers currently loaded. A loaded,
disabled module writes exactly:

```text
mod-world-party-scaling: disabled
```

Also confirm that the core listed the module configuration file:

```bash
docker compose logs ac-worldserver | grep -F "mod_world_party_scaling.conf"
```

The core prints every loaded module config under `Using modules configuration:`.

If the module line is absent, first check that `Logger.module` allows info messages and that the log
covers server startup. If both are fine, the module was not compiled into the image: rebuild
`ac-worldserver`. If the module line is present but the config file is not listed, the `.conf` file is
missing and the built-in defaults are in use.

## Native deployment

1. Confirm that the module is present:

   ```bash
   ls modules/mod-world-party-scaling/src
   ```

2. Configure, compile and install:

   ```bash
   ./acore.sh compiler build
   ```

   The command runs CMake from the configured build directory (`var/build/obj` by default), builds
   `worldserver`, and installs binaries and configuration files into `env/dist`. Because the module is
   static, this step is mandatory after adding or removing it.

3. Check the installed configuration:

   ```bash
   ls env/dist/etc/modules/mod_world_party_scaling.conf*
   ```

   With `AC_ENABLE_CONF_COPY_ON_INSTALL=1` (the default in `conf/dist/config.sh`), the build copies
   the `.conf.dist` to `.conf` when the `.conf` does not exist yet. If only the `.dist` file exists:

   ```bash
   cp env/dist/etc/modules/mod_world_party_scaling.conf.dist \
      env/dist/etc/modules/mod_world_party_scaling.conf
   ```

4. Start the server:

   ```bash
   ./acore.sh run-worldserver
   ```

   or run the binary directly:

   ```bash
   env/dist/bin/worldserver
   ```

5. Apply configuration changes with `.reload config` in game as a GM, `reload config` in the server
   console, or a worldserver restart.

### Verify the native deployment

Check the default log file:

```bash
grep -F "mod-world-party-scaling" env/dist/logs/Server.log
grep -F "mod_world_party_scaling.conf" env/dist/logs/Server.log
```

If `LogsDir` in `worldserver.conf` was changed, use that directory instead. The expected lines are the
same as in the Docker section. The default `Logger.module=4,Console Server` setting routes info-level
module messages to both the console and `Server.log`.

## Enable and disable

`WorldPartyScaling.Enable` is the only switch needed.

### Disable at runtime

1. In `env/dist/etc/modules/mod_world_party_scaling.conf`, set:

   ```ini
   WorldPartyScaling.Enable = 0
   ```

2. Run `.reload config` in game as a GM, or `reload config` in the console.

The reload triggers the module's configuration hook. Every creature currently scaled has its stored
health base restored and its maximum health recomposed, the module state is cleared, and new
engagements are not scaled. A worldserver restart has the same effect.

### Re-enable

1. Set `WorldPartyScaling.Enable = 1`.
2. Run `.reload config` or `reload config`.

The module summary line is written again with the current coefficients. Changes to the health, damage
and XP tier coefficients and to the exclusion lists also take effect through the same reload; no key
requires a restart.

### Inspect debug output

`WorldPartyScaling.EnableDebug = 1` makes the module write one diagnostic line when scaling starts and
one when it is restored. Each line reports the health base flat value, reference health, scaled base
and resulting maximum health, along with the group tier and coefficients. The lines use the `module`
logger at debug level, while the default `Logger.module` is level 4 (info). To see them, raise the logger
in `worldserver.conf`:

```ini
Logger.module=5,Console Server
```

and reload or restart. There is no per-hit logging, so enabling debug on a busy realm does not flood the
log with one line per attack.

## Rollback

### 1. Configuration-only rollback

Use this when the module should stay installed but inactive.

1. Stop scaling with `WorldPartyScaling.Enable = 0`.
2. Reload with `.reload config` or `reload config`.
3. Verify the summary line `mod-world-party-scaling: disabled` in the log.

Active creatures are restored during the reload. There is nothing else to undo: the module never
changed global rates, SQL data or other modules.

### 2. Full removal, native install

1. Stop the worldserver.
2. Remove the module sources:

   ```bash
   rm -rf modules/mod-world-party-scaling
   ```

3. Remove both configuration files:

   ```bash
   rm -f env/dist/etc/modules/mod_world_party_scaling.conf \
         env/dist/etc/modules/mod_world_party_scaling.conf.dist
   ```

   Delete the `.conf` as well. A rebuilt server no longer lists this file, so it would not be read;
   removing it prevents stale settings from being mistaken for active configuration.

4. Reconfigure and rebuild so the module disappears from the static `worldserver` binary:

   ```bash
   ./acore.sh compiler build
   ```

5. Start the worldserver.

### 3. Full removal, Docker install

1. Stop the stack:

   ```bash
   docker compose down
   ```

2. Remove the module sources:

   ```bash
   rm -rf modules/mod-world-party-scaling
   ```

3. Remove both configuration files from the bind-mounted configuration directory:

   ```bash
   rm -f env/dist/etc/modules/mod_world_party_scaling.conf \
         env/dist/etc/modules/mod_world_party_scaling.conf.dist
   ```

4. Rebuild the image and start the stack:

   ```bash
   docker compose build ac-worldserver
   docker compose up -d
   ```

5. Verify that the module line is gone from the fresh log and that the core no longer lists
   `mod_world_party_scaling.conf`.

### 4. Data cleanup

None is required.

- No database table or character column is created or modified.
- No saved variable, cache file or runtime state survives a restart. Runtime state is in memory only,
  and removing a creature restores its stored health base and recomposes maximum health before the
  state is dropped, so grid unload and re-add leaves no scaled health behind.
- No global rate in `worldserver.conf`, `conf/dist/env.ac` or any other module is written.
- Removing the module and its two config files leaves no module residue; the server behaves as it did
  before installation.

### 5. What not to do

- Do not delete only `env/dist/bin/worldserver` or only the module directory while leaving the old
  binary in place: a static build still contains the module until it is rebuilt.
- Do not leave `mod_world_party_scaling.conf` behind after removing the module: the rebuilt server no
  longer reads it, and the stale file only obscures which settings are active.
- Do not expect the module to revert an ongoing scaled fight after the source directory is deleted.
  Disable it first with `Enable = 0` and a reload, or stop the server.

## Deployment checklist

- [ ] `modules/mod-world-party-scaling/src` exists before the build.
- [ ] The build log lists `mod_world_party_scaling.conf` under the modules config list.
- [ ] `env/dist/etc/modules/mod_world_party_scaling.conf` exists and contains the intended values.
- [ ] The worldserver log shows `mod-world-party-scaling: enabled (open world only);` or
      `mod-world-party-scaling: disabled`.
- [ ] `Using modules configuration:` lists `mod_world_party_scaling.conf`.
- [ ] A configuration change applies after `.reload config`, and disabling restores active creatures.
- [ ] A dungeon fight is unaffected by party size.

This guide describes deployment mechanics only. Compilation and test outcomes are recorded in
`TEST_REPORT.md`; they are not claimed here.
