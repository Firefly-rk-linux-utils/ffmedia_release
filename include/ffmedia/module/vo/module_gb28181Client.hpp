/*
 * @Author: dengkx dkx@t-chip.com.cn
 * @Date: 2024-08-27 09:07:55
 * @LastEditors: Kaison Deng dkx@t-chip.com.cn
 * @LastEditTime: 2026-04-09 10:20:20
 * @Description: 输出组件。GB28181客户端，支持推流到GB28181服务器。
 * Copyright (c) 2024-present The ffmedia project authors, All Rights Reserved.
 */
#pragma once
#include "module/module_media.hpp"

#include <cstdint>
#include <functional>

namespace FFMedia
{
class FFMEDIA_API ModuleGB28181Client : public ModuleMedia
{
public:
    using ReceiveHandler = std::function<void(const std::shared_ptr<MediaBuffer>&)>;
    // Handlers run on the SIP/RTP worker paths and must return promptly.
    // Return 0 for success; negative values generate a protocol error.
    using ControlHandler = std::function<int(const std::string& command,
                                             const std::string& deviceId,
                                             const std::string& xml)>;
    using RecordQueryHandler = std::function<int(
        const std::string& deviceId, const std::string& startTime,
        const std::string& endTime, std::string& recordListXml)>;
    using SessionHandler = std::function<int(const std::string& sessionName,
                                             const std::string& deviceId,
                                             int64_t rangeStartUs,
                                             double scale, bool paused)>;

    /**
     * Build the gb28181 client module
     *
     * @param userId         local user identifier.
     * @param userAgent      local user agent.
     * @param transport      sip transport protocol type.
     * @param port           local sip port.
     */
    ModuleGB28181Client(const std::string& userId = "", const std::string& userAgent = "", SIP_TRANSPORT_TYPE transport = TRANSPORT_TYPE_TCP, int port = 0);
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
    int setServerConfig(const std::string& serverId, const std::string& serverRealm, const std::string& serverIp,
                        unsigned short serverPort = 5060, int expiry = 3600);

    /**
     * Set authentication credentials.
     *
     * @param username       username
     * @param password       password
     */
    int setAuthenticationInfo(const std::string& username, const std::string& password);

    /**
     * @description: Set device information.
     * @param {string&} deviceName
     * @param {string&} manufacturer
     * @param {string&} channel         device channel id.
     * @return {int}                    Return 0 on success.
     */
    int setDeviceInfo(const std::string& deviceName, const std::string& manufacturer, const std::string& channel);

    /**
     * This method is used to replace contact address with
     * the public address of your NAT. The ip address should
     * be retreived manually (fixed IP address) or with STUN.
     * This address will only be used when the remote
     * correspondant appears to be on an DIFFERENT LAN.
     *
     * @param firewallip    the ip address.
     */
    int setFirewallip(const std::string& firewallip);
    int setAutoMasquerade(int automasquerade);

    /**
     * Set keepalive duration
     *
     * @param seconds       interval seconds
     */
    int setKeepaliveDuration(int seconds);
    int setReceiveHandler(ReceiveHandler handler);
    int setControlHandler(ControlHandler handler);
    int setRecordQueryHandler(RecordQueryHandler handler);
    int setSessionHandler(SessionHandler handler);
    int setTlsCertificates(const std::string& clientCert,
                           const std::string& clientKey,
                           const std::string& caCert,
                           const std::string& keyPassword = std::string());
    int setTlsVerify(int enabled);

protected:
    ConsumeResult doConsume(const MediaBufferContext& input,
                            std::shared_ptr<MediaBuffer>& output_buffer) override;
    virtual bool setup() override;
    virtual bool teardown() override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace FFMedia
