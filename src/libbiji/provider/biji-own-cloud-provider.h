/* bjb-own-cloud-provider.h
 * Copyright (C) Pierre-Yves LUYTEN 2013 <py@luyten.fr>
 * 
 * bijiben is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * bijiben is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "biji-provider.h"

G_BEGIN_DECLS

#define BIJI_TYPE_OWN_CLOUD_PROVIDER (biji_own_cloud_provider_get_type ())

G_DECLARE_FINAL_TYPE (BijiOwnCloudProvider, biji_own_cloud_provider, BIJI, OWN_CLOUD_PROVIDER, BijiProvider)

BijiProvider     *biji_own_cloud_provider_new           (BijiManager *manager,
                                                         GoaObject *object);


GFile            *biji_own_cloud_provider_get_folder    (BijiOwnCloudProvider *provider);


gchar            *biji_own_cloud_provider_get_readable_path (BijiOwnCloudProvider *p);


G_END_DECLS
