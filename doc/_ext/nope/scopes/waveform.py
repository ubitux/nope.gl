import pynopegl as ngl
from pynopegl_utils.misc import load_media


@ngl.scene()
def waveform(cfg: ngl.SceneCfg):
    image = load_media("rooster")

    stats = ngl.ColorStats(texture=ngl.Texture2D(data_src=ngl.Media(image.filename)))
    return ngl.DrawWaveform(stats, mode="luma_only")
