#ifndef __RTE_JSON_H__
#define __RTE_JSON_H__
enum json_type{
	JSON_NULL,	/* null */
	JSON_FALSE, /* false */
	JSON_TRUE,	/* true */
	JSON_INTEGER,/* 整数 */
	JSON_FLOAT,/* 浮点数 */ 
	JSON_STRING,/* 字符串 */
	JSON_OBJECT,/* 对象 */
	JSON_ARRAY, /* 数组 */
};

/* JSON的值类型 */
struct rte_json{
	struct rte_json *next, *prev; // 用于串联结构类型中并列的成员
	struct rte_json *member; // 指向Object或Array的成员 
	enum json_type type; // 类型
	char *name; //
	union{
		char *val_str;	
		long val_int;
		float val_flt;
	}u;
};

struct rte_json *new_json_item(void);
struct rte_json *rte_parse_json(const char *str);
int rte_destroy_json(struct rte_json *json);
int rte_traverse_json(struct rte_json *json);
char *rte_serialize_json(struct rte_json *json, int fmt);
int rte_array_get_size(struct rte_json *array);
struct rte_json *rte_array_get_item(struct rte_json *array, int idx);
int rte_array_add_item(struct rte_json *array, struct rte_json *item);
int rte_array_del_item(struct rte_json *array, int idx);
struct rte_json *rte_object_get_item(struct rte_json *object, const char *name);
int rte_object_add_item(struct rte_json *object, const char *name, struct rte_json *item);
int rte_object_del_item(struct rte_json *object, const char *name);

#endif
