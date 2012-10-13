htscanner-enhanced
==================

This is a fork of the htscanner project with additional settings to facilitate shared webhosting providers

Who can use this ?
==================

Shared webhosting providers that use mod_fcgi can take benefit from this module.
An example could be cPanel users with a mod_fcgi + Apache setup that wants to enforce some PHP configuration variable on a vhost-basis.

What's wrong with shared webhosting providers ?
===============================================

Sometimes in shared webhosting environments there is a need to set specific php_values for each vhost.

For instance, when using New Relic on a shared hosted environment that is not using php-fpm, this module can help to assign a specific application name for each vhost being served by the webserver.
In fact, New Relic application name can be set in .htaccess like the following :

	<IfModule mod_php5.c>
	php_value newrelic.appname "paoloiannelli.com"
	</IfModule>

This basically mean that shared webhosting providers that want to use New Relic to monitor their customers application, have to include an .htaccess in their users document_root.

Users can, in turn, overwrite this file or make modifications that might affect the data reported to New Relic.

Why this module can provide a fix ?
===================================

With this small modification in htscanner, you can force it to always use the default docroot set in the php.ini configuration.

Getting back to the previous example, assuming you are hosting your vhosts under /home/<username>/public_html, you might set the following parameters :

	htscanner.default_docroot = /home
	htscanner.force_default_docroot = 1

and place a .htaccess inside /home/<username> that cannot be modified/deleted (and even not be read) by the user.

In this way you enforce your settings, without having to mess with the user's "own" .htaccess file.

Of course, the user can override at anytime your settings, but this is not in the scope of this htscanner's modification.

How do I install it ?
=====================

The quick way is the following :

	cd
	git clone https://github.com/piannelli/htscanner-enhanced.git
	cd htscanner-enhanced
	phpize
	./configure
	make && make install

Then add the extension to your php.ini with :

	extension = htscanner.so

and the configuration you want to use (at the end of php.ini), for instance:

	htscanner.config_file = .htaccess
	htscanner.default_docroot = /home
	htscanner.default_ttl = 300
	htscanner.force_default_docroot = 1
	htscanner.stop_on_error = 0
	htscanner.verbose = 0
