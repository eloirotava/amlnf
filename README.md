# amlnf: driver de NAND da Amlogic para kernel moderno

Porte para o Linux 6.12 do driver de NAND crua que a Amlogic distribuía
com o seu kernel 3.10, para o **Meson8 / Meson8b (S805)**. Ele lê e
escreve a NAND no formato do fabricante, o mesmo que o U-Boot de fábrica
entende. Assim, numa TV box S805 dá para usar a NAND sem trocar o
bootloader.

**Testado numa MXQ S805** (Armbian, kernel 6.12.28), com uma NAND MLC
SanDisk SDTNRGAMA de 8 GiB:

- lê e grava as partições do fabricante (`boot`, `system`, `data`,
  `cache`...) como discos `/dev/<nome>`;
- a escrita sobrevive a queda de energia;
- o U-Boot de fábrica carrega um kernel gravado por este driver, e o
  Debian 13 sobe da própria NAND.

Desempenho medido: leitura de 25 MB/s (3,4 MB/s enquanto a FTL faz
coleta de lixo) e escrita de 1,8 a 2 MB/s.

## Por que não o `meson_nand` do kernel

O `meson_nand` do mainline reconhece esta NAND, mas não lê o formato do
fabricante: páginas de 16 KiB com OOB de 1280 bytes esbarram no limite
do modo raw ("too big write size in raw mode"). Mesmo que lesse, a
tradução de blocos (FTL) que o U-Boot de fábrica espera é a da Amlogic.
Este driver existe para quem quer manter o U-Boot e o layout originais.

## O que não é mainline, e não tem como ser

O núcleo da FTL, `nftl/aml_nftl_core_20141222.o`, é um **objeto binário
do fabricante**, de 2014, sem código-fonte. Ele é linkado dentro do
módulo. Todo o resto é código C: o controlador, as partições e a
interface de bloco. Isso já foi adaptado ao kernel atual:

- compila sem nenhum warning, sem flags para escondê-los;
- não depende de cabeçalhos do kernel 3.10 (`<mach/...>`, `<plat/...>`):
  o pouco que restava deles está em `include/amlnf_compat.h`;
- clocks pelo framework de clocks e pinos pelo pinctrl, a partir do
  device tree;
- não exporta símbolos.

O estilo do código do fabricante foi normalizado para o do kernel
(`checkpatch`). A normalização foi verificada comparando o código de
máquina gerado antes e depois, que ficou idêntico.

## Compilar e carregar

```sh
make KDIR=/lib/modules/$(uname -r)/build ARCH=arm
sudo insmod amlnf.ko readonly=0 allow_markbad=0 allow_meta=0 parts=all
```

Para compilação cruzada, acrescente `CROSS_COMPILE=arm-linux-gnueabihf-`
e aponte `KDIR` para os headers do kernel da caixa.

O device tree precisa de um nó `amlogic,meson8b-nfc` (ou
`amlogic,aml_nand`) com a região de registradores, os clocks `core` e
`device` e o estado de pinos `default`.

## Parâmetros, e o que eles protegem

| parâmetro | padrão | efeito |
|---|---|---|
| `readonly` | 1 | recusa toda escrita e todo apagamento |
| `allow_markbad` | 0 | recusa marcar bloco ruim e reescrever a tabela de blocos ruins |
| `allow_meta` | 0 | recusa gravar o metadado do fabricante (`nbbt`, `ncnf`, `nkey`, `nenv`) |
| `parts` | `nfcache` | partições que ganham disco: nomes separados por vírgula, ou `all` |
| `use_cache` | 0 | cache de escrita da FTL (1 foi mais lento nos testes) |

**Deixe `allow_markbad=0` e `allow_meta=0`.** Dado gravado se regrava.
Já um bloco bom marcado como ruim só volta apagando junto o registro dos
blocos que vieram ruins de fábrica. E o `nkey` guarda chaves únicas de
cada aparelho, que nada recupera, nem a ferramenta da Amlogic.

**Cuidado com `readonly=1` numa partição já escrita.** A FTL do
fabricante só abre somente-leitura uma partição limpa. Numa que já foi
escrita, ela precisa reconciliar o mapeamento ao abrir. Com as escritas
recusadas, ela toma a recusa por defeito do chip, começa a marcar
blocos e termina derrubando o kernel. Depois de escrever numa partição,
carregue sempre com `readonly=0`.

## Licença

GPL versão 2 ou posterior (`LICENSE`), como declara o próprio módulo.
O código C vem do driver da Amlogic e do MTD do Linux. O objeto da FTL é
redistribuído como veio no kernel do fabricante.

---

**English.** Out-of-tree port of Amlogic's raw NAND driver for Meson8 /
Meson8b (S805) to Linux 6.12. It reads and writes the vendor NAND
layout, so an S805 box keeps its factory U-Boot and boots Debian from
NAND. The FTL core is a closed vendor object from 2014; the rest is C
cleaned up for a current kernel. Keep `allow_markbad=0` and
`allow_meta=0`, and once a partition has been written, load with
`readonly=0`: the vendor FTL cannot open a written partition read-only.
