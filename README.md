
# Building

Dependencies:
* qt (core, widgets, help, linguist tools)
* samba (smbclient, ndr)
* glib2 (resolv)
* ldap
* krb5
* uuid

Once dependencies are installed, run this from the admc folder:
```
$ mkdir build
$ cd build
$ cmake ..
$ make -j12
```

If the build fails, check build output for missing dependencies.

# Usage:

This app requires a working Active Directory domain and for the client machine to be connected and logged into the domain. You can find articles about these topics on [ALTLinux wiki](https://www.altlinux.org/%D0%94%D0%BE%D0%BC%D0%B5%D0%BD).

Launch admc from the build directory:
```
$ ./admc
```

# Testing

Tests also require a domain and a connection to the domain.

Launch tests from the build directory:
```
$ ./admc-test
```

# Screenshots

![image](https://i.imgur.com/GuRmwnq.png)
