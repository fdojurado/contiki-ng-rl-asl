#include "rl-asl-routing.h"
#include "net/routing/routing.h"
#include "os/sys/log.h"

#define LOG_MODULE "routing"
#define LOG_LEVEL LOG_CONF_LEVEL_RL_ASL_ROUTING

linkaddr_t root_node_addr = {{0x01, 0x00}};

const routing_entry_t routing_table[] = {
    {{{0x02, 0x00}}, {{0x01, 0x00}}, {{0x01, 0x00}}},
    {{{0x03, 0x00}}, {{0x01, 0x00}}, {{0x02, 0x00}}},
    {{{0x04, 0x00}}, {{0x01, 0x00}}, {{0x02, 0x00}}},
    {{{0x05, 0x00}}, {{0x01, 0x00}}, {{0x02, 0x00}}}};

/*---------------------------------------------------------------------------*/
static void init(void)
{
    LOG_INFO("RL ASL routing driver initialized\n");
}
/*---------------------------------------------------------------------------*/
static const linkaddr_t *nexthop(const linkaddr_t *src, const linkaddr_t *dst)
{
    LOG_INFO("Finding next hop from %02x:%02x to %02x:%02x\n",
             src->u8[0], src->u8[1], dst->u8[0], dst->u8[1]);
    for (size_t i = 0; i < sizeof(routing_table) / sizeof(routing_entry_t); i++)
    {
        if (linkaddr_cmp(&routing_table[i].src, src) && linkaddr_cmp(&routing_table[i].dst, dst))
        {
            return &routing_table[i].next_hop;
        }
    }
    LOG_DBG("No next hop found for %02x:%02x to %02x:%02x\n",
            src->u8[0], src->u8[1], dst->u8[0], dst->u8[1]);
    return NULL; // No next hop found
}
/*---------------------------------------------------------------------------*/
static int root_start(void)
{
    LOG_INFO("RL ASL routing root start\n");
    /* In RL ASL, we do not have a routing table, so we just return success */
    return 0;
}
/*---------------------------------------------------------------------------*/
static uint8_t is_in_leaf_mode(void)
{
    /* Do we have any children? */
    int is_leaf = 1;
    for (size_t i = 0; i < sizeof(routing_table) / sizeof(routing_entry_t); i++)
    {
        if (linkaddr_cmp(&routing_table[i].next_hop, &linkaddr_node_addr))
        {
            is_leaf = 0;
            break;
        }
    }
    return is_leaf;
}
/*---------------------------------------------------------------------------*/
const struct routing_driver rl_asl_routing_driver = {
    "rl_asl_routing",
    init,
    nexthop,
    root_start,
    is_in_leaf_mode};
/*---------------------------------------------------------------------------*/
