// SPDX-License-Identifier: GPL-2.0
/* Skip full key/secure providers for M3; FTL only needs the symbols linked. */
#include "../include/phynand.h"

int aml_key_init(struct amlnand_chip *aml_chip)
{
	(void)aml_chip;
	pr_debug("amlnf: aml_key_init stub\n");
	return 0;
}

int aml_secure_init(struct amlnand_chip *aml_chip)
{
	(void)aml_chip;
	pr_debug("amlnf: aml_secure_init stub\n");
	return 0;
}

int aml_nand_update_key(struct amlnand_chip *aml_chip, char *key_ptr)
{
	(void)aml_chip;
	(void)key_ptr;
	return 0;
}

int aml_nand_update_secure(struct amlnand_chip *aml_chip, char *secure_ptr)
{
	(void)aml_chip;
	(void)secure_ptr;
	return 0;
}
