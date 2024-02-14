import pynopegl as ngl


@ngl.scene()
def ngon(cfg: ngl.SceneCfg):
    cfg.aspect_ratio = (4, 3)

    # A live control for the viewer to change the number of sides
    nsides = ngl.UniformInt(value=5, live_id="n", live_min=3, live_max=30)

    shape = ngl.ShapeNGon(n=nsides, radius=1)
    return ngl.DrawColor(color=(1, 0.5, 0), shape=shape, blending="src_over")
