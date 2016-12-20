/*
 * Copyright 2016 libfpta authors: please see AUTHORS file.
 *
 * This file is part of libfpta, aka "Fast Positive Tables".
 *
 * libfpta is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * libfpta is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libfpta.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fast_positive/tables_internal.h"

//----------------------------------------------------------------------------

fpta_value fpta_field2value(const fptu_field *field)
{
    fpta_value result = {fpta_null, 0, {0}};

    if (unlikely(!field))
        return result;

    const fptu_payload *payload = fptu_field_payload(field);
    switch (fptu_get_type(field->ct)) {
    default:
    case fptu_nested:
        result.binary_length = units2bytes(payload->other.varlen.brutto);
        result.binary_data = (void *)payload->other.data;
        result.type = fpta_binary;
        break;

    case fptu_opaque:
        result.binary_length = payload->other.varlen.opaque_bytes;
        result.binary_data = (void *)payload->other.data;
        result.type = fpta_binary;
        break;

    case fptu_null:
        break;

    case fptu_uint16:
        result.type = fpta_unsigned_int;
        result.uint = field->get_payload_uint16();
        break;

    case fptu_int32:
        result.type = fpta_signed_int;
        result.sint = payload->i32;
        break;

    case fptu_uint32:
        result.type = fpta_unsigned_int;
        result.uint = payload->u32;
        break;

    case fptu_fp32:
        result.type = fpta_float_point;
        result.fp = payload->fp32;
        break;

    case fptu_int64:
        result.type = fpta_signed_int;
        result.sint = payload->i64;
        break;

    case fptu_uint64:
        result.type = fpta_unsigned_int;
        result.uint = payload->u64;
        break;

    case fptu_fp64:
        result.type = fpta_float_point;
        result.fp = payload->fp64;
        break;

    case fptu_96:
        result.type = fpta_binary;
        result.binary_length = 96 / 8;
        result.binary_data = (void *)payload->fixbin;
        break;

    case fptu_128:
        result.type = fpta_binary;
        result.binary_length = 128 / 8;
        result.binary_data = (void *)payload->fixbin;
        break;

    case fptu_160:
        result.type = fpta_binary;
        result.binary_length = 160 / 8;
        result.binary_data = (void *)payload->fixbin;
        break;

    case fptu_192:
        result.type = fpta_binary;
        result.binary_length = 192 / 8;
        result.binary_data = (void *)payload->fixbin;
        break;

    case fptu_256:
        result.type = fpta_binary;
        result.binary_length = 256 / 8;
        result.binary_data = (void *)payload->fixbin;
        break;

    case fptu_cstr:
        result.type = fpta_string;
        result.str = payload->cstr;
        result.binary_length = strlen(result.str);
        break;
    }
    return result;
}

int fpta_get_column(fptu_ro row, const fpta_name *column_id,
                    fpta_value *value)
{
    if (unlikely(column_id == nullptr || value == nullptr))
        return FPTA_EINVAL;

    const fptu_field *field = fptu_lookup_ro(row, column_id->column.num,
                                             fpta_name_coltype(column_id));
    *value = fpta_field2value(field);
    return field ? FPTA_SUCCESS : FPTA_NODATA;
}

int fpta_upsert_column(fptu_rw *pt, const fpta_name *column_id,
                       fpta_value value)
{
    if (unlikely(!pt || !fpta_id_validate(column_id, fpta_column)))
        return FPTA_EINVAL;

    fptu_type coltype = fpta_shove2type(column_id->internal);
    assert(column_id->column.num <= fptu_max_cols);
    unsigned col = column_id->column.num;

    switch (coltype) {
    default:
        /* TODO: проверить корректность размера для fptu_farray */
        if (unlikely(value.type != fpta_binary))
            return FPTA_ETYPE;
        return FPTA_ENOIMP;

    case fptu_nested: {
        fptu_ro tuple;
        if (unlikely(value.type != fpta_binary))
            return FPTA_ETYPE;
        tuple.sys.iov_len = value.binary_length;
        tuple.sys.iov_base = value.binary_data;
        return fptu_upsert_nested(pt, col, tuple);
    }

    case fptu_opaque:
        if (unlikely(value.type != fpta_binary))
            return FPTA_ETYPE;
        return fptu_upsert_opaque(pt, col, value.binary_data,
                                  value.binary_length);

    case fptu_null:
        return FPTA_EINVAL;

    case fptu_uint16:
        switch (value.type) {
        default:
            return FPTA_ETYPE;
        case fpta_signed_int:
            if (unlikely(value.sint < 0))
                return FPTA_EVALUE;
        case fpta_unsigned_int:
            if (unlikely(value.uint > UINT16_MAX))
                return FPTA_EVALUE;
        }
        return fptu_upsert_uint16(pt, col, (uint16_t)value.uint);

    case fptu_int32:
        switch (value.type) {
        default:
            return FPTA_ETYPE;
        case fpta_unsigned_int:
            if (unlikely(value.uint > INT32_MAX))
                return FPTA_EVALUE;
        case fpta_signed_int:
            if (unlikely(value.sint != (int32_t)value.sint))
                return FPTA_EVALUE;
        }
        return fptu_upsert_int32(pt, col, (int32_t)value.sint);

    case fptu_uint32:
        switch (value.type) {
        default:
            return FPTA_ETYPE;
        case fpta_signed_int:
            if (unlikely(value.sint < 0))
                return FPTA_EVALUE;
        case fpta_unsigned_int:
            if (unlikely(value.uint > UINT32_MAX))
                return FPTA_EVALUE;
        }
        return fptu_upsert_uint32(pt, col, (uint32_t)value.uint);

    case fptu_fp32:
        if (unlikely(value.type != fpta_float_point))
            return FPTA_ETYPE;
        if (unlikely(std::isnan(value.fp)))
            return FPTA_EVALUE;
        if (unlikely(fabs(value.fp) > FLT_MAX) && !std::isinf(value.fp))
            return FPTA_EVALUE;
        return fptu_upsert_fp32(pt, col, (float)value.fp);

    case fptu_int64:
        switch (value.type) {
        default:
            return FPTA_ETYPE;
        case fpta_unsigned_int:
            if (unlikely(value.uint > INT64_MAX))
                return FPTA_EVALUE;
        case fpta_signed_int:
            break;
        }
        return fptu_upsert_int64(pt, col, value.sint);

    case fptu_uint64:
        switch (value.type) {
        default:
            return FPTA_ETYPE;
        case fpta_signed_int:
            if (unlikely(value.sint < 0))
                return FPTA_EVALUE;
        case fpta_unsigned_int:
            break;
        }
        return fptu_upsert_uint64(pt, col, value.uint);

    case fptu_fp64:
        if (unlikely(value.type != fpta_float_point))
            return FPTA_ETYPE;
        if (unlikely(std::isnan(value.fp)))
            return FPTA_EVALUE;
        return fptu_upsert_fp64(pt, col, value.fp);

    case fptu_96:
        if (unlikely(value.type != fpta_binary))
            return FPTA_ETYPE;
        if (unlikely(value.binary_length != 96 / 8))
            return FPTA_DATALEN_MISMATCH;
        if (unlikely(!value.binary_data))
            return FPTA_EINVAL;
        return fptu_upsert_96(pt, col, value.binary_data);

    case fptu_128:
        if (unlikely(value.type != fpta_binary))
            return FPTA_ETYPE;
        if (unlikely(value.binary_length != 128 / 8))
            return FPTA_DATALEN_MISMATCH;
        if (unlikely(!value.binary_data))
            return FPTA_EINVAL;
        return fptu_upsert_128(pt, col, value.binary_data);

    case fptu_160:
        if (unlikely(value.type != fpta_binary))
            return FPTA_ETYPE;
        if (unlikely(value.binary_length != 160 / 8))
            return FPTA_DATALEN_MISMATCH;
        if (unlikely(!value.binary_data))
            return FPTA_EINVAL;
        return fptu_upsert_160(pt, col, value.binary_data);

    case fptu_192:
        if (unlikely(value.type != fpta_binary))
            return FPTA_ETYPE;
        if (unlikely(value.binary_length != 192 / 8))
            return FPTA_DATALEN_MISMATCH;
        if (unlikely(!value.binary_data))
            return FPTA_EINVAL;
        return fptu_upsert_192(pt, col, value.binary_data);

    case fptu_256:
        if (unlikely(value.type != fpta_binary))
            return FPTA_ETYPE;
        if (unlikely(value.binary_length != 256 / 8))
            return FPTA_DATALEN_MISMATCH;
        if (unlikely(!value.binary_data))
            return FPTA_EINVAL;
        return fptu_upsert_256(pt, col, value.binary_data);

    case fptu_cstr:
        if (unlikely(value.type != fpta_string))
            return FPTA_ETYPE;
        return fptu_upsert_string(pt, col, value.str, value.binary_length);
    }
}

//----------------------------------------------------------------------------

int fpta_put(fpta_txn *txn, fpta_name *table_id, fptu_ro row_value,
             fpta_put_options op)
{
    int rc = fpta_name_refresh_couple(txn, table_id, nullptr);
    if (unlikely(rc != FPTA_SUCCESS))
        return rc;

    unsigned flags = MDB_NODUPDATA;
    switch (op) {
    default:
        return FPTA_EINVAL;
    case fpta_insert:
        if (fpta_index_is_unique(table_id->table.pk))
            flags |= MDB_NOOVERWRITE;
        break;
    case fpta_update:
        flags |= MDB_CURRENT;
        break;
    case fpta_upsert:
        if (!fpta_index_is_unique(table_id->table.pk))
            flags |= MDB_NOOVERWRITE;
        break;
    }

    fpta_key key;
    rc = fpta_index_row2key(table_id->table.pk, 0, row_value, key, false);
    if (unlikely(rc != FPTA_SUCCESS))
        return rc;

    if (unlikely(table_id->mdbx_dbi < 1)) {
        rc = fpta_table_open(txn, table_id, nullptr);
        if (unlikely(rc != FPTA_SUCCESS))
            return rc;
    }

    rc = mdbx_put(txn->mdbx_txn, table_id->mdbx_dbi, &key.mdbx,
                  &row_value.sys, flags);
    return rc;
}

int fpta_del(fpta_txn *txn, fpta_name *table_id, fptu_ro row_value)
{
    int rc = fpta_name_refresh_couple(txn, table_id, nullptr);
    if (unlikely(rc != FPTA_SUCCESS))
        return rc;

    fpta_key key;
    rc = fpta_index_row2key(table_id->table.pk, 0, row_value, key, false);
    if (unlikely(rc != FPTA_SUCCESS))
        return rc;

    if (unlikely(table_id->mdbx_dbi < 1)) {
        rc = fpta_table_open(txn, table_id, nullptr);
        if (unlikely(rc != FPTA_SUCCESS))
            return rc;
    }

    rc = mdbx_del(txn->mdbx_txn, table_id->mdbx_dbi, &key.mdbx,
                  &row_value.sys);
    return rc;
}
