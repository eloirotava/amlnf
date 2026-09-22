# amlnf

Driver de NAND da Amlogic. Antes só para o kernel 3.10. Agora também para kernel moderno.

```
make KDIR=/path/to/kernel ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
insmod amlnf_m3.ko readonly=0 allow_markbad=0 allow_meta=0 parts=all use_cache=0
```

`allow_markbad=0` e `allow_meta=0` ficam fechados. `readonly=0` permite escrever nas partições.
