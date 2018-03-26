MRuby::Gem::Specification.new('mruby-esp32-nokia5110') do |spec|
  spec.license = 'MIT'
  spec.authors = 'icm7216'

  spec.cc.include_paths << "#{build.root}/src"
end
