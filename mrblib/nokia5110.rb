module LCD
  class NOKIA5110
    attr_accessor :color
    attr_accessor :fontsize

    include Constants
    def initialize(options={})
      @color = options[:color] || LCD::WHITE
      @fontsize = options[:fontsize] || 1
      @cs = options[:cs] || CS
      @dc = options[:dc] || DC
      @rst = options[:rst] || RST
      @mosi = options[:mosi] || MOSI
      @sck = options[:sck] || SCK
      @miso = options[:miso] || MISO
      @freq = options[:freq] || SPI_FREQ
      @spi_mode = options[:spi_mode] || SPI_MODE
      @dma_ch = options[:dma_ch] || DMA
      
      _init(@cs, @dc, @rst, @mosi, @sck, @miso, @freq, @spi_mode, @dma_ch)
    end
  end
end
