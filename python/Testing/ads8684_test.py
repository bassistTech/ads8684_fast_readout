import ads8684_fast_readout as afr

if 'ads' in globals():
    ads.close()
port = 'com8'
ads = afr.Ads8684_fast_readout(port)
print('status', ads.status())
print('chans', ads.chans([0, 1, 2, 3]))
print('readTextMode', ads.readTextMode(10))
print('readBinaryMode', ads.readBinaryMode(100))
ads.close()