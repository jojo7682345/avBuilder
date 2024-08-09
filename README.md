# AvBuilder
AvBuilder is a build system written in C that allows for building of both small and large project. At the moment it is just a shitier version of make. 

## How to build

```shell
chmod +x ./bootstrap
./bootstrap
```

## How to install
```shell
./boostrap install [target]
```

## How to use
write a ```.project``` file according to the [syntax](./docs/avBuilderSyntax.md).

## Compiling AvBuilder itself using AvBuilder
```shell
./avBuilder avBuilder.project
```

Run 
```shell
avBuilder [your_project_file.project] [any arguments needed]
```
and it should do its thing.

## Dependencies
### Run dependencies
- ```a working computer``` *(probably)*
### Build dependencies
- ```gcc``` can be changed with some effort
- ```sh``` or ```bash``` 
- ```avUtils``` already included in repository for now
- ```avBuilder``` required to build avBuilder


## Planned features
- Fetching files from the internet
- Refactor to reduce memory usage (it uses an excessive amount at the moment)
- if statements
- propper installation
