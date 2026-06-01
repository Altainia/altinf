#!/bin/sh
# Builds the runtime Wt config from the committed (secret-free) template and,
# when Google credentials are present in the environment, injects the
# google-oauth2-* properties so Wt::Auth::GoogleService picks them up. The
# template at /app/wt_config.xml is never mutated, so this is idempotent across
# container restarts. With the env vars unset, no properties are added and
# Google sign-in stays disabled (an *empty* property would still count as
# configured, hence we add the block only when every value is non-empty).
set -e

# Prefer an operator-supplied config mounted into the /data volume; fall back to
# the secret-free baseline baked into the image. This lets production keep its
# own config (editable via the existing ./data mount, surviving image updates)
# without that file living in git.
TEMPLATE=/app/wt_config.xml
[ -f /data/wt_config.xml ] && TEMPLATE=/data/wt_config.xml
RUNTIME=/tmp/wt_config.xml

cp "$TEMPLATE" "$RUNTIME"

if [ -n "$ALTINF_GOOGLE_CLIENT_ID" ] && \
   [ -n "$ALTINF_GOOGLE_CLIENT_SECRET" ] && \
   [ -n "$ALTINF_GOOGLE_REDIRECT" ]; then

	xml_escape() {
		printf '%s' "$1" | sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g'
	}

	cid=$(xml_escape "$ALTINF_GOOGLE_CLIENT_ID")
	csecret=$(xml_escape "$ALTINF_GOOGLE_CLIENT_SECRET")
	credir=$(xml_escape "$ALTINF_GOOGLE_REDIRECT")

	frag=$(mktemp)
	{
		printf '    <properties>\n'
		printf '      <property name="google-oauth2-redirect-endpoint">%s</property>\n' "$credir"
		printf '      <property name="google-oauth2-client-id">%s</property>\n' "$cid"
		printf '      <property name="google-oauth2-client-secret">%s</property>\n' "$csecret"
		printf '    </properties>\n'
	} > "$frag"

	# Splice the fragment in just before the closing tag.
	awk -v fragfile="$frag" '
		/<\/application-settings>/ {
			while ((getline line < fragfile) > 0) print line
			close(fragfile)
		}
		{ print }
	' "$RUNTIME" > "$RUNTIME.tmp" && mv "$RUNTIME.tmp" "$RUNTIME"
	rm -f "$frag"
fi

exec /app/altinf \
	--config       "$RUNTIME" \
	--docroot      "/app/resources;/css,/js" \
	--approot      /data \
	--http-address 0.0.0.0 --http-port 8080
