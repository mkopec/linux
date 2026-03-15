/*
 * HPO HDMI link sequencing
 *
 * Copyright 2026 Michał Kopeć <michal@nozomi.space>
 */

#ifndef __LINK_HWSS_HPO_HDMI_FRL_H__
#define __LINK_HWSS_HPO_HDMI_FRL_H__

#include "link_hwss.h"
#include "link_service.h"

/*
 * Returns true if the HPO HDMI FRL HWSS can be used for this link.
 *
 * Conditions:
 *  - HDMI signal
 *  - HPO HDMI link encoder present
 *  - FRL rate negotiated (frl_rate != 0)
 */
bool can_use_hpo_hdmi_frl_link_hwss(const struct dc_link *link,
				    const struct link_resource *link_res);

/*
 * Returns the HPO HDMI FRL link_hwss implementation.
 */
const struct link_hwss *get_hpo_hdmi_frl_link_hwss(void);

#endif /* __LINK_HWSS_HPO_HDMI_FRL_H__ */
