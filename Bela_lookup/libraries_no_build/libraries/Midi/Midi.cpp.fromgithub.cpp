
if (device >= 0) {

		for(unsigned int sub = 0; sub < subs; ++sub){
			bool in = false;
			bool out = false;
			int statusOut = is_output(ctl, card, device, sub);
			if (statusOut < 0) {
				error("cannot get rawmidi output information %d:%d:%d: %s",
				card, device, sub, snd_strerror(statusOut));
				return ports;
			}
			out = (statusOut > 0);

			int statusIn = is_input(ctl, card, device, sub);
			if (statusIn < 0) {
				error("cannot get rawmidi input information %d:%d:%d: %s",
				card, device, sub, snd_strerror(statusIn));
				return ports;
			}
			in = (statusIn > 0);

			if(in || out){
				// Refresh ALSA info for this exact subdevice before creating the name.
				snd_rawmidi_info_set_device(info, device);
				snd_rawmidi_info_set_subdevice(info, sub);
				snd_rawmidi_info_set_stream(info, in ? SND_RAWMIDI_STREAM_INPUT : SND_RAWMIDI_STREAM_OUTPUT);
				if ((status = snd_ctl_rawmidi_info(ctl, info)) < 0) {
					error("cannot get rawmidi information %d:%d:%d: %s",
					card, device, sub, snd_strerror(status));
					return ports;
				}
				Port port = getPort(info);
				port.hasInput = in;
				port.hasOutput = out;
				ports.push_back(port);
			}
		}
}
