/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: dengkx dkx@t-chip.com.cn
 * @LastEditTime: 2024-09-09 16:52:58
 * @Description: 输出组件。GB28181客户端，支持推流到GB28181服务器。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once
#include "module/module_media.hpp"

class SipClient;
class ModuleGB28181Client : public ModuleMedia
{
public:
    /**
     * Build the gb28181 client module
     *
     * @param userId         local user identifier.
     * @param userAgent      local user agent.
     * @param transport      sip transport protocol type.
     * @param port           local sip port.
     */
    ModuleGB28181Client(std::string userId, std::string userAgent, SIP_TRANSPORT_TYPE transport = TRANSPORT_TYPE_TCP, int port = 0);
    ~ModuleGB28181Client();
    int init();

    /**
     * Set server configuration
     *
     * @param serverId           The sip server identifier.
     * @param serverRealm        The sip server realm.
     * @param serverIp           The sip server ip address.
     * @param serverPort         The sip server port.
     * @param expiry             The expires value for registration.
     */
    int setServerConfig(std::string serverId, std::string serverRealm, std::string serverIp,
                        unsigned short serverPort = 5060, int expiry = 3600);

    /**
     * Set authentication credentials.
     *
     * @param username       username
     * @param password       password
     */
    int setAuthenticationInfo(std::string username, std::string password);

    /**
     * This method is used to replace contact address with
     * the public address of your NAT. The ip address should
     * be retreived manually (fixed IP address) or with STUN.
     * This address will only be used when the remote
     * correspondant appears to be on an DIFFERENT LAN.
     *
     * @param firewallip    the ip address.
     */
    int setFirewallip(std::string firewallip);
    int setAutoMasquerade(int automasquerade);

    /**
     * Set keepalive duration
     *
     * @param seconds       interval seconds
     */
    int setKeepaliveDuration(int seconds);

protected:
    virtual ConsumeResult doConsume(shared_ptr<MediaBuffer> input_buffer, shared_ptr<MediaBuffer> output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    SipClient* gb;
};