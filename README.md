This is a port of [**Flycast-dojo**](https://github.com/blueminder/flycast-dojo), to run Marvel vs Capcom 2™ on Arcade1Up's Marvel vs Capcom 2™ Arcadce Machine.

# Resource File

Store the rom files and bios files in the config/ROMs/ folder

# Linux Compile Command

```shell
cmake -S{flycast-dir} -B{flycast-build-dir}
```

# Linux Execute Command:

```shell
flycast --config-dir={path-of-flycast-config}/ --data-dir={path-of-flycast-data}/ --game-dir={rom-dir} --player=0 --game-id=0 --room-id=0 --room-ip="" --room-index=0 --user-id=0 --config-changed=false`
```