; Drush Make (http://drupal.org/project/drush_make)
api = 2

; Drupal core

core = 7.x
projects[drupal] = 7

; Dependencies

projects[worker][type] = module
projects[worker][download][type] = git
projects[worker][download][url] = git@github.com:boombatower/worker.git

projects[ctools] = 1
