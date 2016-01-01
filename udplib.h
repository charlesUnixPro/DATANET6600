#define NOLINK  (-1)
int udp_create (char * premote, int32_t * plink);
int udp_release (int32_t link);
int udp_send (int32_t link, uint8_t * pdata, uint16_t count);
int udp_receive (int32_t link, uint8_t * pdata, uint16_t maxbufg);

