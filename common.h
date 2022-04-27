#ifndef CHATTER_COMMON_H_
#define CHATTER_COMMON_H_

#include <sys/socket.h>

namespace chatter {

#define PORT "36547"
#define MAXDATASIZE 100

void* get_in_addr(sockaddr* sa);

} // namespace chatter

#endif // CHATTER_COMMON_H_
