#ifndef __XUSER_H__
#define __XUSER_H__

typedef struct xpeer_callback {
    void *ctx;
    void (*on_task_update)(struct xpeer_callback*, const char *json, int len);
    void (*on_msg_to_peer)(struct xpeer_callback*, unsigned long peer_id, const char *msg, int len);
    void (*on_msg_from_peer)(struct xpeer_callback*, unsigned long peer_id, const char *msg, int len);
    void (*on_frame_to_peer)(struct xpeer_callback*, unsigned long peer_id, int stream_id, const void *frame, int size);
    void (*on_frame_from_peer)(struct xpeer_callback*, unsigned long peer_id, int stream_id, const void *frame, int size);
}xpeer_callback_t;

// 创建 xmsger 网络的 P2P 节点
extern void* xpeer_create(xpeer_callback_t *cb);
// 释放 xmsger 网络的 P2P 节点
extern void xpeer_free(void *peer);

// 向 xmsger 网络请求匿名注册
extern void xpeer_request_anonymous_registration(void *ctx);
// 向 xmsger 网络请求电话号码注册
extern void xpeer_request_phone_number_registration(void *ctx, const char *phone_number);

// 向 xmsger 网络注册一个 P2P 用户 ID
extern void xpeer_user_register(void *ctx, unsigned long user_id, const char *password, const char *public_key, const char *verification_code);
// 向 xmsger 网络注册一个 P2P 用户 ID
extern void xpeer_user_login(void *ctx, unsigned long user_id, const char *password);
// 向 xmsger 网络注册一个 P2P 用户 ID
extern void xpeer_user_logout(void *ctx);
// 向 xmsger 网络删除一个 P2P 用户 ID
extern void xpeer_user_remove(void *ctx);

// 添加指定 ID 为好友
extern void xpeer_user_add_friend(void *ctx, unsigned long peer_id);
// 删除指定 ID 的好友
extern void xpeer_user_remove_friend(void *ctx, unsigned long peer_id);

// 向 指定 ID 注册一个设备，并且设置设备类型
extern void xpeer_user_register_device(void *ctx, unsigned long peer_id, const char *public_key, int type);
// 向 指定 ID 删除一个设备
extern void xpeer_user_remove_device(void *ctx, unsigned long peer_id, const char *public_key);

// 向指定用户ID发送一个消息
extern void xpeer_user_send_msg(void *ctx, unsigned long peer_id, const char *msg, int len);
// 向指定用户ID发送一帧数据
extern void xpeer_user_send_frame(void *ctx, unsigned long peer_id, int stream_id, const unsigned char *frame, int size);

// 向 xmsger 当前用户注册一个群组
extern void xpeer_group_register(void *ctx);
// 向 xmsger 当前用户删除一个群组
extern void xpeer_group_remove(void *ctx, unsigned long group_id);
// 向 xmsger 邀请用户加入群组
extern void xpeer_group_invite(void *ctx, unsigned long group_id, unsigned long friend_peer_id, const char *verification_code);
// 向 xmsger 用户同意加入群组
extern void xpeer_group_join(void *ctx, unsigned long group_id, const char *verification_code);
// 向 xmsger 用户申请加入群组
extern void xpeer_group_apply(void *ctx, unsigned long group_id);
// 向 xmsger 离开群组
extern void xpeer_group_leave(void *ctx, unsigned long group_id);
// 向指定用户ID发送一个消息
extern void xpeer_group_send_msg(void *ctx, unsigned long group_id, const char *msg, int len);
// 向指定用户ID发送一帧数据
extern void xpeer_group_send_frame(void *ctx, unsigned long group_id, int stream_id, const unsigned char *frame, int size);

extern const char* xpeer_get_friend_list(void *ctx);
extern const char* xpeer_get_group_list(void *ctx);
extern const char* xpeer_get_device_list(void *ctx);


#endif //__XUSER_H__