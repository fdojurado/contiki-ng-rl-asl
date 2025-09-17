#include "rl-asl-buf.h"
#include "rl-asl-net-processor.h"
#include <string.h>
#include "os/sys/log.h"

#define LOG_MODULE "rl-asl-buf"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_BUF
/*---------------------------------------------------------------------------*/
static uint16_t rl_asl_buf_attrs[RL_ASL_BUF_ATTR_MAX];
static uint16_t rl_asl_buf_default_attrs[RL_ASL_BUF_ATTR_MAX];
/*---------------------------------------------------------------------------*/
void rl_asl_buf_clear(void)
{
    rl_asl_len = 0;
    rl_asl_buf_clear_attr();
}
/*---------------------------------------------------------------------------*/
bool rl_asl_buf_set_len(uint16_t len)
{
    if (len <= RL_ASL_LINK_MTU)
    {
        rl_asl_len = len;
        return true;
    }
    else
    {
        return false;
    }
}
/*---------------------------------------------------------------------------*/
void rl_asl_set_ip_len_field(struct rl_asl_uip_hdr *hdr, uint16_t len)
{
    if (hdr != NULL)
    {
        hdr->len = len;
    }
}
/*---------------------------------------------------------------------------*/
uint8_t rl_asl_get_ip_len_field(struct rl_asl_uip_hdr *hdr)
{
    if (hdr != NULL)
    {
        return hdr->len;
    }
    else
    {
        return 0;
    }
}
/*---------------------------------------------------------------------------*/
uint8_t *rl_asl_buf_get_ip_next_header(uint8_t *buffer, uint16_t size, uint8_t *protocol)
{
    int curr_hdr_len = 0;
    int next_hdr_len = 0;
    uint8_t *next_header = NULL;
    struct rl_asl_uip_hdr *hdr = NULL;

    hdr = (struct rl_asl_uip_hdr *)buffer;
    *protocol = hdr->proto;
    curr_hdr_len = RL_ASL_IPH_LEN;

    /* Check first if enough space for current header */
    if (curr_hdr_len > size)
    {
        return NULL;
    }

    if (*protocol == RL_ASL_PROTO_DATA)
    {
        next_hdr_len = RL_ASL_DATAH_LEN;
    }
    else
    {
        LOG_ERR("Unknown protocol: %d\n", *protocol);
        return NULL;
    }

    next_header = buffer + curr_hdr_len;
    /* Size must be enough to hold both the current and next header */
    if (next_hdr_len == 0 || curr_hdr_len + next_hdr_len > size)
    {
        return NULL;
    }
    return next_header;
}
/*---------------------------------------------------------------------------*/
uint16_t rl_asl_buf_get_attr(uint8_t type)
{
    if (type < RL_ASL_BUF_ATTR_MAX)
    {
        return rl_asl_buf_attrs[type];
    }
    return 0;
}
/*---------------------------------------------------------------------------*/
int rl_asl_buf_set_attr(uint8_t type, uint16_t value)
{
    if (type < RL_ASL_BUF_ATTR_MAX)
    {
        rl_asl_buf_attrs[type] = value;
        return 1; // Success
    }
    return 0; // Error: invalid type
}
/*---------------------------------------------------------------------------*/
int rl_asl_buf_set_default_attr(uint8_t type, uint16_t value)
{
    if (type < RL_ASL_BUF_ATTR_MAX)
    {
        rl_asl_buf_default_attrs[type] = value;
        return 1; // Success
    }
    return 0; // Error: invalid type
}
/*---------------------------------------------------------------------------*/
void rl_asl_buf_clear_attr(void)
{
    memcpy(rl_asl_buf_attrs, rl_asl_buf_default_attrs, sizeof(rl_asl_buf_attrs));
}
/*---------------------------------------------------------------------------*/
void rl_asl_buf_init(void)
{
    memset(rl_asl_buf_default_attrs, 0, sizeof(rl_asl_buf_default_attrs));
    rl_asl_buf_set_default_attr(RL_ASL_BUF_ATTR_MAX_MAC_TRANSMISSIONS,
                                RL_ASL_MAX_MAC_TRANSMISSIONS_UNDEFINED);
    rl_asl_buf_set_default_attr(RL_ASL_BUF_ATTR_LLSEC_LEVEL,
                                RL_ASL_BUF_ATTR_LLSEC_LEVEL_MAC_DEFAULT);
}