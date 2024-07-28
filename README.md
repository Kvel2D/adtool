# Active Directory Management Center (ADMC)

The ADMC is an integrated complex tool that implements
User and Computers and Group Policy Manager modules of
Microsoft Remote Server Administration Tools (RSAT).

![Screenshot](https://i.imgur.com/GuRmwnq.png)

## Building

Dependencies:
* qt (core, widgets, help, linguist tools)
* samba (smbclient, ndr)
* glibc (resolv)
* ldap
* krb5
* uuid

Once dependencies are installed, run this from the admc folder:
```sh
mkdir build
cd build
cmake ..
make -j12
```

If the build fails, check build output for missing dependencies.

You can also format the sources by building `clangformat`
target after `cmake` is run, for example:

```sh
make -C build clangformat
```

## Usage:

This app requires a working Active Directory domain and for the client machine to be connected and logged into the domain.
You can find articles about these topics on [ALTLinux wiki](https://www.altlinux.org/%D0%94%D0%BE%D0%BC%D0%B5%D0%BD).

Launch admc from the build directory:
```sh
./admc
```

## Testing

Tests also require a domain and a connection to the domain.

Launch tests from the build directory:
```sh
./admc-test
```

