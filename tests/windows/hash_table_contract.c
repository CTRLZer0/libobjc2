/*
 * SPDX-License-Identifier: MIT
 * Copyright (C) 2026 CTRLZer0 contributors and applicable copyright holders.
 * See COPYING and NOTICE.md for licensing and provenance details.
 */
#include "test_support.h"
#include <stdint.h>
#include <windows.h>

struct contract_value
{
	uintptr_t key;
	uintptr_t payload;
};

static struct contract_value null_value;
static int value_is_null(struct contract_value value) { return value.key == 0; }
static int value_matches(const void *key, struct contract_value value)
{
	return (uintptr_t)key == value.key;
}
static uint32_t key_hash(const void *key) { return (((uint32_t)(uintptr_t)key) >> 2) * UINT32_C(2654435761); }
static uint32_t value_hash(struct contract_value value) { return ((uint32_t)value.key >> 2) * UINT32_C(2654435761); }

#define MAP_TABLE_NAME contract
#define MAP_TABLE_COMPARE_FUNCTION value_matches
#define MAP_TABLE_VALUE_TYPE struct contract_value
#define MAP_TABLE_VALUE_NULL value_is_null
#define MAP_TABLE_HASH_KEY key_hash
#define MAP_TABLE_HASH_VALUE value_hash
#define MAP_TABLE_VALUE_PLACEHOLDER null_value
#define MAP_TABLE_ACCESS_BY_REFERENCE 1
#include "hash_table.h"

struct worker_context { contract_table *table; };
static DWORD WINAPI insert_worker(void *opaque)
{
	struct worker_context *context = opaque;
	struct contract_value value = {99, 9900};
	return contract_insert(context->table, value) ? 0 : 1;
}

static int check_value(contract_table *table, uintptr_t key, uintptr_t payload)
{
	struct contract_value *value = contract_table_get(table, (void*)key);
	return (value != NULL) && (value->key == key) && (value->payload == payload);
}

int main(void)
{
	size_t bytes = 0;
	CHECK(objc2_size_multiply(7, 9, &bytes) && bytes == 63);
	CHECK(!objc2_size_multiply(SIZE_MAX, 2, &bytes));
	contract_table *zero = (contract_table *)(uintptr_t)1;
	contract_initialize(&zero, 0);
	CHECK(zero == NULL);

	contract_table *table = NULL;
	contract_initialize(&table, 128);
	CHECK(table != NULL);
	struct contract_value first = {1, 10};
	CHECK(contract_insert(table, first));
	contract_remove(table, (void*)999);
	struct worker_context context = {table};
	HANDLE worker = CreateThread(NULL, 0, insert_worker, &context, 0, NULL);
	CHECK(worker != NULL);
	DWORD wait = WaitForSingleObject(worker, 1000);
	CHECK(wait == WAIT_OBJECT_0);
	DWORD exitCode = 1;
	CHECK(GetExitCodeThread(worker, &exitCode));
	CloseHandle(worker);
	CHECK(exitCode == 0);
	CHECK(check_value(table, 99, 9900));

	struct contract_value inserted = {1000, 1};
	contract_table_set(table, (void*)1000, inserted);
	CHECK(check_value(table, 1000, 1));
	inserted.payload = 2;
	contract_table_set(table, (void*)1000, inserted);
	CHECK(check_value(table, 1000, 2));

	contract_table *stress = NULL;
	contract_initialize(&stress, 128);
	CHECK(stress != NULL);
	for (uintptr_t key = 1; key <= 80; ++key)
	{
		struct contract_value value = {key, key * 10};
		if (!contract_insert(stress, value)) { fprintf(stderr, "stress insert failed at key %llu\n", (unsigned long long)key); return 1; }
	}
	for (uintptr_t key = 1; key <= 80; ++key)
	{
		CHECK(check_value(stress, key, key * 10));
		if (key & 1) { contract_remove(stress, (void*)key); }
	}
	for (uintptr_t key = 1; key <= 80; ++key)
	{
		struct contract_value *value = contract_table_get(stress, (void*)key);
		if (key & 1) { CHECK(value == NULL); }
		else { CHECK(value != NULL && value->payload == key * 10); }
	}
	for (uintptr_t key = 1; key <= 80; key += 2)
	{
		struct contract_value value = {key, key * 100};
		if (!contract_insert(stress, value)) { fprintf(stderr, "stress insert failed at key %llu\n", (unsigned long long)key); return 1; }
		CHECK(check_value(stress, key, key * 100));
	}

	return 0;
}
