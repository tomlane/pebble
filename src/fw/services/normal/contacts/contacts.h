/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "attributes_address.h"

#include "util/attributes.h"
#include "util/uuid.h"

typedef struct {
  Uuid id;
  uint32_t flags;
  AttributeList attr_list;
  AddressList addr_list;
} Contact;

//! Lookup a contact given its uuid. Will return NULL if no contact is found.
//! The contact must be freed with contacts_free_contact().
Contact* contacts_get_contact_by_uuid(const Uuid *uuid);

//! Frees a contact
void contacts_free_contact(Contact *contact);
