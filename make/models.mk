# Shared firmware/host model preparation. Keep the original .nam as the source.
A2_HEADER = build/generated/embedded_a2_data.h
A2_MODELS = models/local/fender-twin65-a2-lite.nam models/local/vox-ac30-chimey-a2-lite.nam models/local/marshall-jcm800-g5-a2-lite.nam

$(A2_MODELS):
	@echo 'Missing required amp models. Run bash scripts/download_models.sh first.' >&2
	@exit 1

$(A2_HEADER): $(A2_MODELS) scripts/convert_a2.py
	python3 scripts/convert_a2.py $@
