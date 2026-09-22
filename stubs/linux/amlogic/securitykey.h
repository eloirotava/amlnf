/* Minimal stub of Amlogic securitykey provider API for OOT build. */
#ifndef _STUB_LINUX_AMLOGIC_SECURITYKEY_H
#define _STUB_LINUX_AMLOGIC_SECURITYKEY_H

#include <linux/types.h>

struct aml_keybox_provider;

typedef struct aml_keybox_provider {
	char *name;
	int32_t (*read)(struct aml_keybox_provider *p, uint8_t *buf, int len, int flags);
	int32_t (*write)(struct aml_keybox_provider *p, uint8_t *buf, int len);
	void *priv;
} aml_keybox_provider_t;

typedef struct {
	uint32_t dummy;
} meson_key;

static inline int aml_keybox_provider_register(aml_keybox_provider_t *p)
{
	(void)p;
	return 0;
}

static inline aml_keybox_provider_t *aml_keybox_provider_get(const char *name)
{
	(void)name;
	return NULL;
}

#endif
