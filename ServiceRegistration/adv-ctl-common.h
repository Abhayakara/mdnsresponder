//
//  xpc_client_advertising_proxy.h
//  mDNSResponder
//
//  Copyright (c) 2019 Apple Inc. All rights reserved.
//

#ifndef XPC_CLIENT_ADVERTISING_PROXY_H
#define XPC_CLIENT_ADVERTISING_PROXY_H

#define kDNSSDAdvertisingProxyResponse         0
#define kDNSSDAdvertisingProxyEnable           1
#define kDNSSDAdvertisingProxyListServiceTypes 2
#define kDNSSDAdvertisingProxyListServices     3
#define kDNSSDAdvertisingProxyListHosts        4
#define kDNSSDAdvertisingProxyGetHost          5
#define kDNSSDAdvertisingProxyFlushEntries     6
#define kDNSSDAdvertisingProxyBlockService     7
#define kDNSSDAdvertisingProxyUnblockService   8
#define kDNSSDAdvertisingProxyRegenerateULA    9

#endif /* XPC_CLIENT_ADVERTISING_PROXY_H */

// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 108
// indent-tabs-mode: nil
// End:
