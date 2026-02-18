std::vector<Midi::Port> Midi::listAllPorts(){
	std::vector<Port> ports;
	int card = -1;
	int status;
	while((status = snd_card_next(&card)) == 0){
		if(card < 0){
			break;
		}
		snd_ctl_t *ctl;
		char name[32];
		int device = -1;
		int status;
		sprintf(name, "hw:%d", card);
		if ((status = snd_ctl_open(&ctl, name, 0)) < 0) {
			error("cannot open control for card %d: %s\n", card, snd_strerror(status));
			return ports;
		}
		do {
			status = snd_ctl_rawmidi_next_device(ctl, &device);
			if (status < 0) {
				error("cannot determine device number: %s", snd_strerror(status));
				break;
			}
			if (device >= 0) {
				snd_rawmidi_info_t *info;
				snd_rawmidi_info_alloca(&info);
				snd_rawmidi_info_set_device(info, device);
				// count subdevices:
				snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_INPUT);
				snd_ctl_rawmidi_info(ctl, info);
				unsigned int subs_in = snd_rawmidi_info_get_subdevices_count(info);
				snd_rawmidi_info_set_stream(info, SND_RAWMIDI_STREAM_OUTPUT);
				snd_ctl_rawmidi_info(ctl, info);
				unsigned int subs_out = snd_rawmidi_info_get_subdevices_count(info);
				//number of subdevices is max (inputs, outputs);
				unsigned int subs = subs_in > subs_out ? subs_in : subs_out;

				for(unsigned int sub = 0; sub < subs; ++sub){
					bool in = false;
					bool out = false;
					if ((status = is_output(ctl, card, device, sub)) < 0) {
						error("cannot get rawmidi information %d:%d: %s",
						card, device, snd_strerror(status));
						return ports;
					} else if (status){
						out = true;
						// writeTo
					}

					if (status == 0) {
						if ((status = is_input(ctl, card, device, sub)) < 0) {
							error("cannot get rawmidi information %d:%d: %s",
							card, device, snd_strerror(status));
							return ports;
						}
					} else if (status) {
						in = true;
						// readfrom
					}

					if(in || out){
						Port port = getPort(info);
						port.hasInput = in;
						port.hasOutput = out;
						ports.push_back(port);
					}
				}
			}
		} while (device >= 0);
		snd_ctl_close(ctl);
	}
	return ports;
}
