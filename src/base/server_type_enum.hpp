/*
 * Copyright 2008 Search Solution Corporation
 * Copyright 2021 CUBRID Corporation
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

#ifndef _SERVER_TYPE_ENUM_H_
#define _SERVER_TYPE_ENUM_H_

typedef enum
{
  SERVER_TYPE_UNKNOWN,
  SERVER_TYPE_TRANSACTION,
  SERVER_TYPE_PAGE,
  SERVER_TYPE_ANY,
} SERVER_TYPE;

enum class server_type_config
{
  TRANSACTION,
  PAGE,
  SINGLE_NODE,
};
#endif // _SERVER_TYPE_ENUM_H_
